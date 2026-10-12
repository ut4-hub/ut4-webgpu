// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineUserSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
const FUniqueNetId& GetEmptySimplyStreamUserId()
{
	return *FUniqueNetIdSimplyStream::EmptyId();
}
}

// FOnlineUserSimplyStream implementation

FOnlineUserSimplyStream::FOnlineUserSimplyStream(const FString& InUserId, const FString& InUsername, const FString& InDisplayName, const FString& InPlatform, const FString& InAvatarUrl, bool bInIsGuest)
	: UserId(FUniqueNetIdSimplyStream::Create(InUserId))
	, Username(InUsername)
	, DisplayName(InDisplayName)
	, Platform(InPlatform)
	, AvatarUrl(InAvatarUrl)
	, bIsGuest(bInIsGuest)
{
	// Populate user attributes for extensibility
	UserAttributes.Add(TEXT("Username"), Username);
	UserAttributes.Add(TEXT("DisplayName"), DisplayName);
	UserAttributes.Add(TEXT("Platform"), Platform);
	UserAttributes.Add(TEXT("AvatarUrl"), AvatarUrl);
	UserAttributes.Add(TEXT("IsGuest"), bIsGuest ? TEXT("true") : TEXT("false"));
}

FUniqueNetIdRef FOnlineUserSimplyStream::GetUserId() const
{
	check(UserId.IsValid());
	return UserId.ToSharedRef();
}

FString FOnlineUserSimplyStream::GetRealName() const
{
	return DisplayName.IsEmpty() ? Username : DisplayName;
}

FString FOnlineUserSimplyStream::GetDisplayName(const FString& InPlatform) const
{
	return DisplayName.IsEmpty() ? Username : DisplayName;
}

bool FOnlineUserSimplyStream::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = UserAttributes.Find(AttrName);
	if (FoundAttr != nullptr)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

bool FOnlineUserSimplyStream::SetUserLocalAttribute(const FString& AttrName, const FString& InAttrValue)
{
	UserAttributes.Add(AttrName, InAttrValue);
	return true;
}

// FOnlineUserInterfaceSimplyStream implementation

FOnlineUserInterfaceSimplyStream::FOnlineUserInterfaceSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
	: Subsystem(InSubsystem)
{
}

bool FOnlineUserInterfaceSimplyStream::QueryUserInfo(int32 LocalUserNum, const TArray<FUniqueNetIdRef>& UserIds)
{
	if (UserIds.Num() == 0)
	{
		UE_LOG_ONLINE_USER(Warning, TEXT("QueryUserInfo: No user IDs provided"));
		TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, false, UserIds, TEXT("No user IDs provided"));
		return false;
	}

	FString AuthToken = GetJWTToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_USER(Warning, TEXT("QueryUserInfo: No auth token for local user %d"), LocalUserNum);
		TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, false, UserIds, TEXT("Not authenticated"));
		return false;
	}

	// Build comma-separated list of user IDs
	TArray<FString> UserIdStrings;
	for (const FUniqueNetIdRef& UserId : UserIds)
	{
		UserIdStrings.Add(UserId->ToString());
	}
	FString UserIdsParam = FString::Join(UserIdStrings, TEXT(","));

	// Build request URL
	FString URL = FString::Printf(TEXT("%s/api/v1/multiplayer/player/users/bulk?userIds=%s"),
		*GetAPIBaseURL(),
		*FGenericPlatformHttp::UrlEncode(UserIdsParam));

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineUserInterfaceSimplyStream::OnQueryUserInfoComplete, LocalUserNum, UserIds);

	UE_LOG_ONLINE_USER(Verbose, TEXT("QueryUserInfo: Querying %d users"), UserIds.Num());

	return HttpRequest->ProcessRequest();
}

void FOnlineUserInterfaceSimplyStream::OnQueryUserInfoComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 LocalUserNum, TArray<FUniqueNetIdRef> UserIds)
{
	bool bSuccess = false;
	FString ErrorStr;

	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		FString ResponseContent = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			bSuccess = JsonObject->GetBoolField(TEXT("success"));
			if (bSuccess)
			{
				const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;
				if (JsonObject->TryGetArrayField(TEXT("results"), ResultsArray))
				{
					FScopeLock Lock(&CachedUsersLock);

					// Get or create cache for this local user
					TUniqueNetIdMap<TSharedRef<FOnlineUser>>& UserCache = CachedUsers.FindOrAdd(LocalUserNum);

					for (const TSharedPtr<FJsonValue>& ResultValue : *ResultsArray)
					{
						const TSharedPtr<FJsonObject>* ResultObject = nullptr;
						if (ResultValue->TryGetObject(ResultObject) && ResultObject->IsValid())
						{
							FString UserId = (*ResultObject)->GetStringField(TEXT("userId"));
							FString Username = (*ResultObject)->GetStringField(TEXT("username"));
							FString DisplayName = (*ResultObject)->GetStringField(TEXT("displayName"));
							FString Platform = (*ResultObject)->GetStringField(TEXT("platform"));
							FString AvatarUrl;
							(*ResultObject)->TryGetStringField(TEXT("avatarUrl"), AvatarUrl);
							bool bIsGuest = (*ResultObject)->GetBoolField(TEXT("isGuest"));

							// Create user object
							TSharedRef<FOnlineUserSimplyStream> User = MakeShared<FOnlineUserSimplyStream>(UserId, Username, DisplayName, Platform, AvatarUrl, bIsGuest);

							// Cache the user
							FUniqueNetIdPtr UserNetId = FUniqueNetIdSimplyStream::Create(UserId);
							if (UserNetId.IsValid())
							{
								UserCache.Add(UserNetId.ToSharedRef(), User);
							}

							UE_LOG_ONLINE_USER(Verbose, TEXT("Cached user: %s (%s)"), *DisplayName, *UserId);
						}
					}

					UE_LOG_ONLINE_USER(Log, TEXT("QueryUserInfo: Cached %d users"), ResultsArray->Num());
				}
				else
				{
					ErrorStr = TEXT("Missing results array");
					bSuccess = false;
				}
			}
			else
			{
				JsonObject->TryGetStringField(TEXT("error"), ErrorStr);
			}
		}
		else
		{
			ErrorStr = TEXT("Failed to parse response JSON");
		}
	}
	else
	{
		ErrorStr = FString::Printf(TEXT("HTTP request failed: %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
	}

	if (!bSuccess)
	{
		UE_LOG_ONLINE_USER(Warning, TEXT("QueryUserInfo failed: %s"), *ErrorStr);
	}

	TriggerOnQueryUserInfoCompleteDelegates(LocalUserNum, bSuccess, UserIds, ErrorStr);
}

bool FOnlineUserInterfaceSimplyStream::GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<FOnlineUser>>& OutUsers)
{
	FScopeLock Lock(&CachedUsersLock);

	const TUniqueNetIdMap<TSharedRef<FOnlineUser>>* UserCache = CachedUsers.Find(LocalUserNum);
	if (UserCache != nullptr)
	{
		UserCache->GenerateValueArray(OutUsers);
		return true;
	}

	return false;
}

TSharedPtr<FOnlineUser> FOnlineUserInterfaceSimplyStream::GetUserInfo(int32 LocalUserNum, const FUniqueNetId& UserId)
{
	FScopeLock Lock(&CachedUsersLock);

	const TUniqueNetIdMap<TSharedRef<FOnlineUser>>* UserCache = CachedUsers.Find(LocalUserNum);
	if (UserCache != nullptr)
	{
		const TSharedRef<FOnlineUser>* FoundUser = UserCache->Find(UserId.AsShared());
		if (FoundUser != nullptr)
		{
			return *FoundUser;
		}
	}

	return nullptr;
}

bool FOnlineUserInterfaceSimplyStream::QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate)
{
	if (DisplayNameOrEmail.IsEmpty())
	{
		UE_LOG_ONLINE_USER(Warning, TEXT("QueryUserIdMapping: Empty search query"));
		Delegate.ExecuteIfBound(false, UserId, DisplayNameOrEmail, GetEmptySimplyStreamUserId(), TEXT("Search query cannot be empty"));
		return false;
	}

	// Get auth token for the requesting user
	FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(Subsystem->GetIdentityInterface(), UserId);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_USER(Warning, TEXT("QueryUserIdMapping: No auth token for user"));
		Delegate.ExecuteIfBound(false, UserId, DisplayNameOrEmail, GetEmptySimplyStreamUserId(), TEXT("Not authenticated"));
		return false;
	}

	// Build request URL
	FString URL = FString::Printf(TEXT("%s/api/v1/multiplayer/player/users/search?query=%s"),
		*GetAPIBaseURL(),
		*FGenericPlatformHttp::UrlEncode(DisplayNameOrEmail));

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineUserInterfaceSimplyStream::OnQueryUserIdMappingComplete, UserId.AsShared(), DisplayNameOrEmail, Delegate);

	UE_LOG_ONLINE_USER(Verbose, TEXT("QueryUserIdMapping: Searching for user '%s'"), *DisplayNameOrEmail);

	return HttpRequest->ProcessRequest();
}

void FOnlineUserInterfaceSimplyStream::OnQueryUserIdMappingComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef RequestingUserId, FString SearchQuery, FOnQueryUserMappingComplete Delegate)
{
	bool bSuccess = false;
	FString ErrorStr;
	FUniqueNetIdPtr FoundUserId = nullptr;

	if (bWasSuccessful && Response.IsValid())
	{
		int32 ResponseCode = Response->GetResponseCode();
		FString ResponseContent = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

		if (ResponseCode == 200 && FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			bSuccess = JsonObject->GetBoolField(TEXT("success"));
			if (bSuccess)
			{
				const TSharedPtr<FJsonObject>* ResultObject = nullptr;
				if (JsonObject->TryGetObjectField(TEXT("result"), ResultObject) && ResultObject->IsValid())
				{
					FString UserId = (*ResultObject)->GetStringField(TEXT("userId"));
					FoundUserId = FUniqueNetIdSimplyStream::Create(UserId);

					UE_LOG_ONLINE_USER(Log, TEXT("QueryUserIdMapping: Found user '%s' with ID %s"), *SearchQuery, *UserId);
				}
				else
				{
					ErrorStr = TEXT("Missing result object");
					bSuccess = false;
				}
			}
			else
			{
				JsonObject->TryGetStringField(TEXT("error"), ErrorStr);
			}
		}
		else if (ResponseCode == 404)
		{
			// User not found - this is a valid response, not an error
			ErrorStr = TEXT("User not found");
			UE_LOG_ONLINE_USER(Verbose, TEXT("QueryUserIdMapping: User '%s' not found"), *SearchQuery);
		}
		else
		{
			ErrorStr = FString::Printf(TEXT("HTTP error: %d"), ResponseCode);
		}
	}
	else
	{
		ErrorStr = FString::Printf(TEXT("HTTP request failed: %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
	}

	if (!bSuccess && !ErrorStr.IsEmpty())
	{
		UE_LOG_ONLINE_USER(Warning, TEXT("QueryUserIdMapping failed: %s"), *ErrorStr);
	}

	const FUniqueNetId& ResolvedId = FoundUserId.IsValid() ? *FoundUserId : GetEmptySimplyStreamUserId();
	Delegate.ExecuteIfBound(bSuccess, *RequestingUserId, SearchQuery, ResolvedId, ErrorStr);
}

bool FOnlineUserInterfaceSimplyStream::QueryExternalIdMappings(const FUniqueNetId& UserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate)
{
	UE_LOG_ONLINE_USER(Warning, TEXT("QueryExternalIdMappings: Not implemented"));
	Delegate.ExecuteIfBound(false, UserId, QueryOptions, ExternalIds, TEXT("Not implemented"));
	return false;
}

void FOnlineUserInterfaceSimplyStream::GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<FUniqueNetIdPtr>& OutIds)
{
	UE_LOG_ONLINE_USER(Warning, TEXT("GetExternalIdMappings: Not implemented"));
	// Return empty array
}

FUniqueNetIdPtr FOnlineUserInterfaceSimplyStream::GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId)
{
	UE_LOG_ONLINE_USER(Warning, TEXT("GetExternalIdMapping: Not implemented"));
	return nullptr;
}

FString FOnlineUserInterfaceSimplyStream::GetAPIBaseURL() const
{
	return Subsystem->GetAPIBaseURL();
}

FString FOnlineUserInterfaceSimplyStream::GetJWTToken(int32 LocalUserNum) const
{
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
		return IdentityInt->GetAuthToken(LocalUserNum);
	}
	return FString();
}

FUniqueNetIdPtr FOnlineUserInterfaceSimplyStream::GetUniquePlayerId(int32 LocalUserNum) const
{
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
		return IdentityInt->GetUniquePlayerId(LocalUserNum);
	}
	return nullptr;
}
