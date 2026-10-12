// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineSubsystemSimplyStreamTypes.h"
#include "Misc/Guid.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Http.h"
#include "HttpModule.h"
#include "Json.h"
#include "JsonObjectConverter.h"

/** User account details */
class FUserOnlineAccountSimplyStream : public FUserOnlineAccount
{
public:
	FUniqueNetIdSimplyStreamPtr UserId;
	FString UserName;
	FString DisplayName;
	FString AuthToken;
	FString RefreshToken;
	FString Platform;
	bool bIsGuest;

	FUserOnlineAccountSimplyStream(const FString& InUserId = FString(),
	                                const FString& InToken = FString(),
	                                const FString& InProjectId = FString(),
	                                const FString& InPlatform = FString(),
	                                bool bInIsGuest = false)
		: UserId(MakeShared<FUniqueNetIdSimplyStream>(InUserId, InProjectId, InPlatform, bInIsGuest))
		, AuthToken(InToken)
		, Platform(InPlatform)
		, bIsGuest(bInIsGuest)
	{
	}

	virtual ~FUserOnlineAccountSimplyStream() = default;

	// FUserOnlineAccount
	virtual FUniqueNetIdRef GetUserId() const override { return UserId.ToSharedRef(); }
	virtual FString GetRealName() const override { return DisplayName; }
	virtual FString GetDisplayName(const FString& InPlatform = FString()) const override { return DisplayName; }
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		if (AttrName == TEXT("Platform"))
		{
			OutAttrValue = Platform;
			return true;
		}
		else if (AttrName == TEXT("IsGuest"))
		{
			OutAttrValue = bIsGuest ? TEXT("true") : TEXT("false");
			return true;
		}
		return false;
	}
	virtual bool SetUserAttribute(const FString& AttrName, const FString& AttrValue) override { return false; }
	virtual FString GetAccessToken() const override { return AuthToken; }
	virtual bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const override 
	{
		if (AttrName == TEXT("RefreshToken"))
		{
			OutAttrValue = RefreshToken;
			return true;
		}
		return false;
	}
};

FOnlineIdentitySimplyStream::FOnlineIdentitySimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
	: Subsystem(InSubsystem)
{
}

bool FOnlineIdentitySimplyStream::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	FString ErrorStr;
	bool bStartedLogin = false;

	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		ErrorStr = FString::Printf(TEXT("Invalid LocalUserNum=%d"), LocalUserNum);
	}
	else if (AccountCredentials.Id.IsEmpty())
	{
		ErrorStr = TEXT("AccountCredentials.Id is empty");
	}
	else
	{
		// Already logged in is not a new login completion. CommonUser checks GetLoginStatus before
		// starting AutoLogin; rebroadcasting a successful login here can wake it after its active
		// login request has finished and re-enter session flows such as QuickPlay.
		if (GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
		{
			UE_LOG(LogOnline, Log, TEXT("Login ignored for user %d: already logged in"), LocalUserNum);
			return true;
		}

		// Coalesce concurrent logins: Lyra's init flow can request login twice (observed: two /auth/guest
		// requests -> two OnLoginComplete broadcasts; the late one fired CommonUser's
		// ensure(LocalUserInfo->IsDoingLogin())). The in-flight request's single completion serves both callers.
		if (LoginsInFlight.Contains(LocalUserNum))
		{
			UE_LOG(LogOnline, Log, TEXT("Login already in flight for user %d — coalescing"), LocalUserNum);
			return true;
		}

		// Prepare HTTP request
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
		
		FString Url = TEXT("https://simplystream.com/api/v1/multiplayer/auth/");
		FString RequestBody;
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

		// Determine auth type from AccountCredentials.Type
		if (AccountCredentials.Type == TEXT("guest"))
		{
			Url += TEXT("guest");
			JsonObject->SetStringField(TEXT("projectId"), AccountCredentials.Id);
		}
		else if (AccountCredentials.Type == TEXT("steam") || 
				 AccountCredentials.Type == TEXT("epic") ||
				 AccountCredentials.Type == TEXT("xbox") ||
				 AccountCredentials.Type == TEXT("playstation"))
		{
			Url += TEXT("login");
			JsonObject->SetStringField(TEXT("projectId"), AccountCredentials.Id);
			JsonObject->SetStringField(TEXT("platform"), AccountCredentials.Type);
			JsonObject->SetStringField(TEXT("externalId"), AccountCredentials.Token);
			JsonObject->SetStringField(TEXT("externalToken"), AccountCredentials.Token);
		}
		else // Default to SimplyStream native login
		{
			Url += TEXT("login");
			JsonObject->SetStringField(TEXT("projectId"), AccountCredentials.Id);
			JsonObject->SetStringField(TEXT("platform"), TEXT("simplystream"));
			JsonObject->SetStringField(TEXT("username"), AccountCredentials.Id);
			JsonObject->SetStringField(TEXT("password"), AccountCredentials.Token);
		}

		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

		HttpRequest->SetURL(Url);
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		HttpRequest->SetContentAsString(RequestBody);

		HttpRequest->OnProcessRequestComplete().BindLambda([this, LocalUserNum, ReqProjectId = AccountCredentials.Id](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			LoginsInFlight.Remove(LocalUserNum);

			FString ErrorStr;
			FString UserId;
			FString AuthToken;
			FString RefreshToken;
			FString UserName;
			FString DisplayName;
			bool bIsGuest = false;

			if (bWasSuccessful && Response.IsValid())
			{
				if (Response->GetResponseCode() == 200 || Response->GetResponseCode() == 201)
				{
					TSharedPtr<FJsonObject> JsonResponse;
					TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

					if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
					{
						// The backend wraps success payloads as { "success": true, "result": { ... } }. Unwrap
						// "result" (falling back to the top level for any unwrapped responses); otherwise
						// userId/token parse empty and login fails despite the 201, which Lyra's CommonUser then
						// surfaces as "TryToInitializeUser failed".
						TSharedPtr<FJsonObject> Data = JsonResponse;
						const TSharedPtr<FJsonObject>* ResultObj = nullptr;
						if (JsonResponse->TryGetObjectField(TEXT("result"), ResultObj) && ResultObj && ResultObj->IsValid())
						{
							Data = *ResultObj;
						}

						Data->TryGetStringField(TEXT("userId"), UserId);
						Data->TryGetStringField(TEXT("token"), AuthToken);
						Data->TryGetStringField(TEXT("refreshToken"), RefreshToken);
						Data->TryGetStringField(TEXT("username"), UserName);
						Data->TryGetStringField(TEXT("displayName"), DisplayName);
						Data->TryGetBoolField(TEXT("isGuest"), bIsGuest);

						if (!UserId.IsEmpty() && !AuthToken.IsEmpty())
						{
							// Extract platform and project ID from the request
							FString Platform;
							FString ProjectId;
							Data->TryGetStringField(TEXT("platform"), Platform);
							Data->TryGetStringField(TEXT("projectId"), ProjectId);
							// The guest-login response omits projectId; fall back to the one we authenticated with, so the
							// account (and its FUniqueNetId) carry a valid ProjectId. OnlineSessionSimplyStream needs it —
							// ExtractProjectId(HostingPlayerId) otherwise returns empty and every CreateSession/FindSessions
							// bails with "Hosting player lacks project ID" before making any HTTP call.
							if (ProjectId.IsEmpty())
							{
								ProjectId = ReqProjectId;
							}

							// Create user account with full metadata
							TSharedRef<FUserOnlineAccountSimplyStream> UserAccount = MakeShareable(
								new FUserOnlineAccountSimplyStream(UserId, AuthToken, ProjectId, Platform, bIsGuest)
							);
							UserAccount->UserName = UserName;
							UserAccount->DisplayName = DisplayName;
							UserAccount->RefreshToken = RefreshToken;

							// Store the account
							UserAccounts.Add(LocalUserNum, UserAccount);
							UserIds.Add(LocalUserNum, UserAccount->UserId);

							TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserAccount->UserId, TEXT(""));
							UE_LOG(LogOnline, Log, TEXT("Login successful for user %d: %s"), LocalUserNum, *UserId);
							return;
						}
					}
				}
				else
				{
					ErrorStr = FString::Printf(TEXT("Login failed with code %d"), Response->GetResponseCode());
				}
			}
			else
			{
				ErrorStr = TEXT("No response from login request");
			}

			// Create empty ID for failure case
			TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdSimplyStream::Create(TEXT("")), ErrorStr);
			UE_LOG(LogOnline, Warning, TEXT("Login failed for user %d: %s"), LocalUserNum, *ErrorStr);
		});

		LoginsInFlight.Add(LocalUserNum);
		HttpRequest->ProcessRequest();
		bStartedLogin = true;
	}

	if (!bStartedLogin)
	{
		// Defer, same as the already-logged-in path: no synchronous completion broadcasts from inside Login.
		Subsystem->ExecuteNextTick([this, LocalUserNum, ErrorStr]()
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdSimplyStream::Create(TEXT("")), ErrorStr);
		});
	}

	return bStartedLogin;
}

bool FOnlineIdentitySimplyStream::Logout(int32 LocalUserNum)
{
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		return false;
	}

	FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		// Remove the user account
		UserAccounts.Remove(LocalUserNum);
		UserIds.Remove(LocalUserNum);

		TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
		return true;
	}

	TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	return false;
}

bool FOnlineIdentitySimplyStream::AutoLogin(int32 LocalUserNum)
{
	// Lyra's CommonUser (COMMONUSER_OSSV1=1) drives login through IOnlineIdentity::AutoLogin (AutoLoginOSSv1 ->
	// IdentityInterface->AutoLogin), NOT Login-with-credentials. While this returned false the client showed
	// "Failed to Request Login" with no network call and no log. Perform a guest login: the projectId comes from
	// the launcher-injected PROJECT_ID env (simplystream.js sets it from window.state.project_id) or, as a
	// fallback, the cooked [SimplyStreamTargetSettings] ProjectID.
	FString ProjectId = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_ID"));
	if (ProjectId.IsEmpty())
	{
		// Browser client: simplystream.js appends -SimplyStreamProjectId=<id> to Module.arguments (from the
		// launcher's window.state.project_id), which lands on the wasm command line.
		FParse::Value(FCommandLine::Get(), TEXT("SimplyStreamProjectId="), ProjectId);
	}
	if (ProjectId.IsEmpty() && GConfig)
	{
		GConfig->GetString(
			TEXT("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings"),
			TEXT("ProjectID"), ProjectId, GEngineIni);
	}
	if (ProjectId.IsEmpty())
	{
		UE_LOG(LogOnline, Warning, TEXT("FOnlineIdentitySimplyStream::AutoLogin: no PROJECT_ID env, -SimplyStreamProjectId= arg, or [SimplyStreamTargetSettings] ProjectID; cannot guest-login user %d"), LocalUserNum);
		return false;
	}

	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("guest");
	Credentials.Id = ProjectId;
	Credentials.Token = FString();
	UE_LOG(LogOnline, Log, TEXT("FOnlineIdentitySimplyStream::AutoLogin: guest login user %d for project %s"), LocalUserNum, *ProjectId);
	return Login(LocalUserNum, Credentials);
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentitySimplyStream::GetUserAccount(const FUniqueNetId& UserId) const
{
	for (const auto& Entry : UserAccounts)
	{
		if (*Entry.Value->GetUserId() == UserId)
		{
			return Entry.Value;
		}
	}
	return nullptr;
}

TArray<TSharedPtr<FUserOnlineAccount>> FOnlineIdentitySimplyStream::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount>> Result;
	for (const auto& Entry : UserAccounts)
	{
		Result.Add(Entry.Value);
	}
	return Result;
}

FUniqueNetIdPtr FOnlineIdentitySimplyStream::GetUniquePlayerId(int32 LocalUserNum) const
{
	const FUniqueNetIdPtr* UserId = UserIds.Find(LocalUserNum);
	if (UserId && UserId->IsValid())
	{
		return *UserId;
	}
	return nullptr;
}

FUniqueNetIdPtr FOnlineIdentitySimplyStream::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Bytes && Size == sizeof(FGuid))
	{
		FGuid Guid;
		FMemory::Memcpy(&Guid, Bytes, sizeof(FGuid));
		return FUniqueNetIdSimplyStream::Create(Guid.ToString());
	}
	return nullptr;
}

FUniqueNetIdPtr FOnlineIdentitySimplyStream::CreateUniquePlayerId(const FString& Str)
{
	return FUniqueNetIdSimplyStream::Create(Str);
}

ELoginStatus::Type FOnlineIdentitySimplyStream::GetLoginStatus(int32 LocalUserNum) const
{
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		return ELoginStatus::NotLoggedIn;
	}

	if (UserAccounts.Contains(LocalUserNum))
	{
		return ELoginStatus::LoggedIn;
	}

	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentitySimplyStream::GetLoginStatus(const FUniqueNetId& UserId) const
{
	for (const auto& Entry : UserAccounts)
	{
		if (*Entry.Value->GetUserId() == UserId)
		{
			return ELoginStatus::LoggedIn;
		}
	}
	return ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentitySimplyStream::GetPlayerNickname(int32 LocalUserNum) const
{
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		return FString();
	}

	const TSharedRef<FUserOnlineAccount>* UserAccount = UserAccounts.Find(LocalUserNum);
	if (UserAccount)
	{
		return (*UserAccount)->GetDisplayName();
	}

	return FString();
}

FString FOnlineIdentitySimplyStream::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(UserId);
	if (UserAccount.IsValid())
	{
		return UserAccount->GetDisplayName();
	}
	return FString();
}

FString FOnlineIdentitySimplyStream::GetAuthToken(int32 LocalUserNum) const
{
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		return FString();
	}

	const TSharedRef<FUserOnlineAccount>* UserAccount = UserAccounts.Find(LocalUserNum);
	if (UserAccount)
	{
		return (*UserAccount)->GetAccessToken();
	}

	return FString();
}

void FOnlineIdentitySimplyStream::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate, EShowPrivilegeResolveUI ShowResolveUI)
{
	// For now, assume all logged in users have all privileges
	if (GetLoginStatus(UserId) == ELoginStatus::LoggedIn)
	{
		Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
	}
	else
	{
		Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::UserNotLoggedIn);
	}
}

FPlatformUserId FOnlineIdentitySimplyStream::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	for (const auto& Entry : UserIds)
	{
		if (Entry.Value.IsValid() && *Entry.Value == UniqueNetId)
		{
			return FPlatformUserId::CreateFromInternalId(Entry.Key);
		}
	}
	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentitySimplyStream::GetAuthType() const
{
	return TEXT("SimplyStream");
}

void FOnlineIdentitySimplyStream::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	// Not implemented yet
	Delegate.ExecuteIfBound(UserId, FOnlineError(false));
}

#if UE_SERVER
void FOnlineIdentitySimplyStream::InitDedicatedServerLogin()
{
	if (!IsRunningDedicatedServer())
	{
		return;
	}

	if (bDedicatedServerAuthInProgress || bDedicatedServerAuthSucceeded)
	{
		return;
	}

	const FString ServerId = FPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_ID"));
	const FString ApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("API_KEY"));
	FString ProjectId = FPlatformMisc::GetEnvironmentVariable(TEXT("PROJECT_ID"));

	if (ProjectId.IsEmpty() && GConfig)
	{
		GConfig->GetString(
			TEXT("/Script/SimplyStreamTargetPlatformSettings.SimplyStreamTargetSettings"),
			TEXT("ProjectID"),
			ProjectId,
			GEngineIni);
	}

	if (ServerId.IsEmpty() || ApiKey.IsEmpty())
	{
		UE_LOG(LogOnline, Warning, TEXT("Dedicated server SimplyStream login skipped: SERVER_ID or API_KEY missing"));
		return;
	}

	const FString ServerName = FPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_NAME"));

	bDedicatedServerAuthInProgress = true;

	TSharedRef<FUserOnlineAccountSimplyStream> UserAccount = MakeShareable(
		new FUserOnlineAccountSimplyStream(ServerId, ApiKey, ProjectId, TEXT("server"), false));
	UserAccount->UserName = ServerId;
	UserAccount->DisplayName = ServerName.IsEmpty() ? ServerId : ServerName;

	UserAccounts.Add(0, UserAccount);
	UserIds.Add(0, UserAccount->UserId);
	bDedicatedServerAuthSucceeded = true;
	bDedicatedServerAuthInProgress = false;

	TriggerOnLoginCompleteDelegates(0, true, *UserAccount->UserId, TEXT(""));
	UE_LOG(LogOnline, Log, TEXT("Dedicated server authenticated with SimplyStream via environment credentials (server_id=%s, project_id=%s)"),
		*ServerId, *ProjectId);
}
#endif
