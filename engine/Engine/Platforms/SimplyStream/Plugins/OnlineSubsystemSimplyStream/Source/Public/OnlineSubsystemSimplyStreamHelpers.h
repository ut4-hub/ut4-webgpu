// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformMisc.h"
#include "Interfaces/OnlineIdentityInterface.h"

namespace SimplyStreamIdentity
{
	inline int32 GetLocalUserNumFromUniqueNetId(const IOnlineIdentityPtr& IdentityPtr, const FUniqueNetId& UserId)
	{
		if (!IdentityPtr.IsValid())
		{
			return INDEX_NONE;
		}

		const FPlatformUserId PlatformUserId = IdentityPtr->GetPlatformUserIdFromUniqueNetId(UserId);
		if (PlatformUserId == PLATFORMUSERID_NONE)
		{
			return INDEX_NONE;
		}

		return IdentityPtr->GetLocalUserNumFromPlatformUserId(PlatformUserId);
	}

	inline FString GetServerApiKeyFallback()
	{
#if UE_SERVER
		FString ApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("API_KEY"));
		if (ApiKey.IsEmpty())
		{
			ApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("SIMPLYSTREAM_API_KEY"));
		}
		return ApiKey;
#else
		return FString();
#endif
	}

	inline FString GetAuthTokenForUser(const IOnlineIdentityPtr& IdentityPtr, const FUniqueNetId& UserId)
	{
		FString AuthToken;

		const int32 LocalUserNum = GetLocalUserNumFromUniqueNetId(IdentityPtr, UserId);
		if (LocalUserNum != INDEX_NONE && IdentityPtr.IsValid())
		{
			AuthToken = IdentityPtr->GetAuthToken(LocalUserNum);
		}

		if (AuthToken.IsEmpty())
		{
			AuthToken = GetServerApiKeyFallback();
		}

		return AuthToken;
	}
}
