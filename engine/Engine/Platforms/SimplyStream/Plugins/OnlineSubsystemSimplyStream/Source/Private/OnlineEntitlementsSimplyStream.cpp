// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineEntitlementsSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ScopeLock.h"

FOnlineEntitlementsSimplyStream::FOnlineEntitlementsSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
	: Subsystem(InSubsystem)
{
}

TSharedPtr<FOnlineEntitlement> FOnlineEntitlementsSimplyStream::GetEntitlement(const FUniqueNetId& UserId, const FUniqueEntitlementId& EntitlementId)
{
	FScopeLock ScopeLock(&EntitlementsCacheLock);

	const FUniqueNetIdRef UserIdRef = UserId.AsShared();
	const TArray<TSharedRef<FOnlineEntitlement>>* UserEntitlements = CachedEntitlements.Find(UserIdRef);

	if (UserEntitlements)
	{
		for (const TSharedRef<FOnlineEntitlement>& Entitlement : *UserEntitlements)
		{
			if (Entitlement->Id == EntitlementId)
			{
				return Entitlement;
			}
		}
	}

	return nullptr;
}

TSharedPtr<FOnlineEntitlement> FOnlineEntitlementsSimplyStream::GetItemEntitlement(const FUniqueNetId& UserId, const FString& ItemId)
{
	FScopeLock ScopeLock(&EntitlementsCacheLock);

	const FUniqueNetIdRef UserIdRef = UserId.AsShared();
	const TArray<TSharedRef<FOnlineEntitlement>>* UserEntitlements = CachedEntitlements.Find(UserIdRef);

	if (UserEntitlements)
	{
		for (const TSharedRef<FOnlineEntitlement>& Entitlement : *UserEntitlements)
		{
			if (Entitlement->ItemId == ItemId)
			{
				return Entitlement;
			}
		}
	}

	return nullptr;
}

void FOnlineEntitlementsSimplyStream::GetAllEntitlements(const FUniqueNetId& UserId, const FString& Namespace, TArray<TSharedRef<FOnlineEntitlement>>& OutUserEntitlements)
{
	FScopeLock ScopeLock(&EntitlementsCacheLock);

	const FUniqueNetIdRef UserIdRef = UserId.AsShared();
	const TArray<TSharedRef<FOnlineEntitlement>>* UserEntitlements = CachedEntitlements.Find(UserIdRef);

	if (UserEntitlements)
	{
		// Filter by namespace if specified
		if (Namespace.IsEmpty())
		{
			OutUserEntitlements = *UserEntitlements;
		}
		else
		{
			for (const TSharedRef<FOnlineEntitlement>& Entitlement : *UserEntitlements)
			{
				if (Entitlement->Namespace == Namespace)
				{
					OutUserEntitlements.Add(Entitlement);
				}
			}
		}
	}
}

bool FOnlineEntitlementsSimplyStream::QueryEntitlements(const FUniqueNetId& UserId, const FString& Namespace, const FPagedQuery& Page)
{
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		UE_LOG_ONLINE_ENTITLEMENT(Warning, TEXT("QueryEntitlements: Identity interface unavailable"));
		Subsystem->ExecuteNextTick([this, UserIdRef = UserId.AsShared(), Namespace]()
		{
			TriggerOnQueryEntitlementsCompleteDelegates(false, *UserIdRef, Namespace, TEXT("Identity interface unavailable"));
		});
		return false;
	}

	const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
	if (IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
	{
		UE_LOG_ONLINE_ENTITLEMENT(Warning, TEXT("QueryEntitlements: User not logged in"));
		Subsystem->ExecuteNextTick([this, UserIdRef = UserId.AsShared(), Namespace]()
		{
			TriggerOnQueryEntitlementsCompleteDelegates(false, *UserIdRef, Namespace, TEXT("User not logged in"));
		});
		return false;
	}

	const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_ENTITLEMENT(Warning, TEXT("QueryEntitlements: Missing auth token"));
		Subsystem->ExecuteNextTick([this, UserIdRef = UserId.AsShared(), Namespace]()
		{
			TriggerOnQueryEntitlementsCompleteDelegates(false, *UserIdRef, Namespace, TEXT("Missing auth token"));
		});
		return false;
	}

	// Build URL with query parameters
	FString URL = Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/player/entitlements");

	TArray<FString> QueryParams;
	if (!Namespace.IsEmpty())
	{
		QueryParams.Add(FString::Printf(TEXT("namespace=%s"), *FGenericPlatformHttp::UrlEncode(Namespace)));
	}

	// Add pagination parameters
	const int32 Limit = FMath::Clamp(Page.Count, 1, 100);
	const int32 Offset = FMath::Max(0, Page.Start);
	QueryParams.Add(FString::Printf(TEXT("limit=%d"), Limit));
	QueryParams.Add(FString::Printf(TEXT("offset=%d"), Offset));

	if (QueryParams.Num() > 0)
	{
		URL += TEXT("?") + FString::Join(QueryParams, TEXT("&"));
	}

	// Create HTTP request
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(
		this,
		&FOnlineEntitlementsSimplyStream::OnQueryEntitlementsComplete,
		UserId.AsShared(),
		Namespace,
		FOnQueryEntitlementsCompleteDelegate());

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG_ONLINE_ENTITLEMENT(Warning, TEXT("QueryEntitlements: Failed to send HTTP request"));
		Subsystem->ExecuteNextTick([this, UserIdRef = UserId.AsShared(), Namespace]()
		{
			TriggerOnQueryEntitlementsCompleteDelegates(false, *UserIdRef, Namespace, TEXT("Failed to send HTTP request"));
		});
		return false;
	}

	return true;
}

void FOnlineEntitlementsSimplyStream::OnQueryEntitlementsComplete(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bWasSuccessful,
	FUniqueNetIdRef UserId,
	FString Namespace,
	FOnQueryEntitlementsCompleteDelegate Delegate)
{
	bool bSuccess = false;
	FString ErrorString;

	if (bWasSuccessful && Response.IsValid())
	{
		if (Response->GetResponseCode() == 200)
		{
			FString ResponseContent = Response->GetContentAsString();
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
				if (bApiSuccess)
				{
					const TArray<TSharedPtr<FJsonValue>>* ResultsArray = nullptr;
					if (JsonObject->TryGetArrayField(TEXT("results"), ResultsArray) && ResultsArray)
					{
						TArray<TSharedRef<FOnlineEntitlement>> Entitlements;
						for (const TSharedPtr<FJsonValue>& EntitlementValue : *ResultsArray)
						{
							const TSharedPtr<FJsonObject>* EntitlementObject = nullptr;
							if (EntitlementValue->TryGetObject(EntitlementObject) && EntitlementObject)
							{
								Entitlements.Add(CreateEntitlementFromJson(*EntitlementObject));
							}
						}

						// Cache the entitlements
						CacheEntitlements(UserId, Entitlements);
						bSuccess = true;

						UE_LOG_ONLINE_ENTITLEMENT(Verbose, TEXT("QueryEntitlements: Retrieved %d entitlements"), Entitlements.Num());
					}
				}
				else
				{
					ErrorString = JsonObject->HasField(TEXT("error")) ? JsonObject->GetStringField(TEXT("error")) : TEXT("API returned success=false");
				}
			}
			else
			{
				ErrorString = TEXT("Failed to parse JSON response");
			}
		}
		else
		{
			ErrorString = FString::Printf(TEXT("HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
		}
	}
	else
	{
		ErrorString = TEXT("HTTP request failed");
	}

	if (!bSuccess)
	{
		UE_LOG_ONLINE_ENTITLEMENT(Warning, TEXT("QueryEntitlements failed: %s"), *ErrorString);
	}

	// Trigger delegates
	Delegate.ExecuteIfBound(bSuccess, *UserId, Namespace, ErrorString);
	TriggerOnQueryEntitlementsCompleteDelegates(bSuccess, *UserId, Namespace, ErrorString);
}

TSharedRef<FOnlineEntitlement> FOnlineEntitlementsSimplyStream::CreateEntitlementFromJson(const TSharedPtr<FJsonObject>& JsonEntitlement) const
{
	TSharedRef<FOnlineEntitlement> Entitlement = MakeShared<FOnlineEntitlement>();

	Entitlement->Id = JsonEntitlement->GetStringField(TEXT("entitlementId"));
	Entitlement->Name = JsonEntitlement->GetStringField(TEXT("name"));
	Entitlement->ItemId = JsonEntitlement->GetStringField(TEXT("itemId"));
	Entitlement->Namespace = JsonEntitlement->GetStringField(TEXT("namespace"));
	Entitlement->bIsConsumable = JsonEntitlement->GetBoolField(TEXT("isConsumable"));
	Entitlement->RemainingCount = JsonEntitlement->GetIntegerField(TEXT("remainingCount"));

	if (JsonEntitlement->HasField(TEXT("consumedCount")))
	{
		Entitlement->ConsumedCount = JsonEntitlement->GetIntegerField(TEXT("consumedCount"));
	}

	if (JsonEntitlement->HasField(TEXT("startDate")))
	{
		Entitlement->StartDate = JsonEntitlement->GetStringField(TEXT("startDate"));
	}

	if (JsonEntitlement->HasField(TEXT("endDate")) && !JsonEntitlement->GetField<EJson::None>(TEXT("endDate"))->IsNull())
	{
		Entitlement->EndDate = JsonEntitlement->GetStringField(TEXT("endDate"));
	}

	if (JsonEntitlement->HasField(TEXT("status")))
	{
		Entitlement->Status = JsonEntitlement->GetStringField(TEXT("status"));
	}

	return Entitlement;
}

void FOnlineEntitlementsSimplyStream::CacheEntitlements(const FUniqueNetIdRef& UserId, const TArray<TSharedRef<FOnlineEntitlement>>& Entitlements)
{
	FScopeLock ScopeLock(&EntitlementsCacheLock);
	CachedEntitlements.Add(UserId, Entitlements);
}
