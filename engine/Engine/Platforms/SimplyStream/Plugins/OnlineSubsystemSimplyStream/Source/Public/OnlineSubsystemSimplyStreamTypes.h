// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemSimplyStreamPackage.h"
#include "IPAddress.h"
#include "Misc/Crc.h"

#ifndef SIMPLYSTREAM_SUBSYSTEM
#define SIMPLYSTREAM_SUBSYSTEM FName(TEXT("SimplyStream"))
#endif

class FUniqueNetIdSimplyStream;

using FUniqueNetIdSimplyStreamPtr = TSharedPtr<const FUniqueNetIdSimplyStream>;
using FUniqueNetIdSimplyStreamRef = TSharedRef<const FUniqueNetIdSimplyStream>;

/**
 * SimplyStream-specific unique net ID that can carry additional metadata
 * beyond the simple string representation used by FUniqueNetIdString.
 * This allows us to embed user IDs, project IDs, platform hints, and other
 * contextual information needed for backend API calls.
 */
class FUniqueNetIdSimplyStream : public FUniqueNetId
{
private:
	/** The unique user ID string (e.g., "user_abc123") */
	FString UserId;

	/** The project ID this user belongs to */
	FString ProjectId;

	/** Platform hint (e.g., "steam", "epic", "simplystream", "guest") */
	FString Platform;

	/** Whether this is a guest account */
	bool bIsGuest;

	/** Cached hash for performance */
	mutable uint32 CachedHash;

	/** Whether hash has been calculated */
	mutable bool bHashCached;

	/** Cached UTF-8 bytes of UserId; GetBytes() returns a stable pointer into this (TCHAR_TO_UTF8 is a
	 *  temporary, so returning its internal buffer directly is a dangling pointer). */
	mutable TArray<uint8> CachedUtf8Bytes;

	template <typename... TArgs>
	static FUniqueNetIdSimplyStreamPtr MakeSimplyStreamId(TArgs&&... Args)
	{
		return MakeShared<FUniqueNetIdSimplyStream>(Forward<TArgs>(Args)...);
	}

public:
	/**
	 * Construct a SimplyStream unique ID
	 * @param InUserId The unique user ID string
	 * @param InProjectId The project ID (optional)
	 * @param InPlatform The platform identifier (optional)
	 * @param bInIsGuest Whether this is a guest account
	 */
	FUniqueNetIdSimplyStream(const FString& InUserId = FString(),
	                         const FString& InProjectId = FString(),
	                         const FString& InPlatform = FString(),
	                         bool bInIsGuest = false)
		: UserId(InUserId)
		, ProjectId(InProjectId)
		, Platform(InPlatform)
		, bIsGuest(bInIsGuest)
		, CachedHash(0)
		, bHashCached(false)
	{
	}

	// FUniqueNetId interface
	virtual FName GetType() const override
	{
		return SIMPLYSTREAM_SUBSYSTEM;
	}

	virtual const uint8* GetBytes() const override
	{
		// For serialization we expose the UTF-8 UserId bytes. Cache them so the returned pointer outlives the
		// call — TCHAR_TO_UTF8 builds a temporary whose buffer would otherwise dangle at the semicolon.
		if (CachedUtf8Bytes.Num() == 0 && !UserId.IsEmpty())
		{
			FTCHARToUTF8 Utf8(*UserId);
			CachedUtf8Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		}
		return CachedUtf8Bytes.GetData();
	}

	virtual int32 GetSize() const override
	{
		// Size in bytes of the UTF-8 encoded user ID
		return FCStringAnsi::Strlen(TCHAR_TO_UTF8(*UserId));
	}

	virtual bool IsValid() const override
	{
		return !UserId.IsEmpty();
	}

	virtual uint32 GetTypeHash() const override
	{
		if (!bHashCached)
		{
			const uint32 UserHash = UserId.IsEmpty() ? 0u : FCrc::StrCrc32(*UserId);
			const uint32 ProjectHash = ProjectId.IsEmpty() ? 0u : FCrc::StrCrc32(*ProjectId);
			const uint32 PlatformHash = Platform.IsEmpty() ? 0u : FCrc::StrCrc32(*Platform);
			CachedHash = HashCombine(HashCombine(UserHash, ProjectHash), PlatformHash);
			bHashCached = true;
		}
		return CachedHash;
	}

	virtual FString ToString() const override
	{
		return UserId;
	}

	virtual FString ToDebugString() const override
	{
		if (ProjectId.IsEmpty() && Platform.IsEmpty())
		{
			return FString::Printf(TEXT("SimplyStream:%s%s"),
				*UserId,
				bIsGuest ? TEXT(" (guest)") : TEXT(""));
		}

		return FString::Printf(TEXT("SimplyStream:%s [Project:%s, Platform:%s]%s"),
			*UserId,
			*ProjectId,
			*Platform,
			bIsGuest ? TEXT(" (guest)") : TEXT(""));
	}

	// SimplyStream-specific accessors
	const FString& GetUserId() const { return UserId; }
	const FString& GetProjectId() const { return ProjectId; }
	const FString& GetPlatform() const { return Platform; }
	bool IsGuest() const { return bIsGuest; }

	// Setters for mutable properties
	void SetProjectId(const FString& InProjectId) { ProjectId = InProjectId; bHashCached = false; }
	void SetPlatform(const FString& InPlatform) { Platform = InPlatform; bHashCached = false; }
	void SetIsGuest(bool bInIsGuest) { bIsGuest = bInIsGuest; }

	/**
	 * Factory method to create a shared pointer to this ID type
	 */
	static FUniqueNetIdPtr Create(const FString& InUserId,
	                              const FString& InProjectId = FString(),
	                              const FString& InPlatform = FString(),
	                              bool bInIsGuest = false)
	{
		return StaticCastSharedPtr<const FUniqueNetId>(MakeSimplyStreamId(InUserId, InProjectId, InPlatform, bInIsGuest));
	}

	static FUniqueNetIdSimplyStreamPtr CreateTyped(const FString& InUserId,
	                                               const FString& InProjectId = FString(),
	                                               const FString& InPlatform = FString(),
	                                               bool bInIsGuest = false)
	{
		return MakeSimplyStreamId(InUserId, InProjectId, InPlatform, bInIsGuest);
	}

	static FUniqueNetIdSimplyStreamRef CreateRef(const FString& InUserId,
	                                             const FString& InProjectId = FString(),
	                                             const FString& InPlatform = FString(),
	                                             bool bInIsGuest = false)
	{
		return MakeSimplyStreamId(InUserId, InProjectId, InPlatform, bInIsGuest).ToSharedRef();
	}

	/**
	 * Create from an existing generic FUniqueNetId (extracts the string representation)
	 */
	static FUniqueNetIdPtr CreateFrom(const FUniqueNetId& OtherId)
	{
		return StaticCastSharedPtr<const FUniqueNetId>(CreateTypedFrom(OtherId));
	}

	static FUniqueNetIdSimplyStreamPtr CreateTypedFrom(const FUniqueNetId& OtherId)
	{
		if (OtherId.GetType() == SIMPLYSTREAM_SUBSYSTEM)
		{
			const FUniqueNetIdSimplyStream* SimplyStreamId = static_cast<const FUniqueNetIdSimplyStream*>(&OtherId);
			if (SimplyStreamId)
			{
				return MakeShared<FUniqueNetIdSimplyStream>(*SimplyStreamId);
			}
		}
		return MakeSimplyStreamId(OtherId.ToString());
	}

	static FUniqueNetIdSimplyStreamRef CreateRefFrom(const FUniqueNetId& OtherId)
	{
		return CreateTypedFrom(OtherId).ToSharedRef();
	}

	static FUniqueNetIdSimplyStreamRef EmptyId()
	{
		static const FUniqueNetIdSimplyStreamRef Empty = CreateRef(TEXT(""));
		return Empty;
	}

	FUniqueNetIdSimplyStreamRef AsShared() const
	{
		return StaticCastSharedRef<const FUniqueNetIdSimplyStream>(FUniqueNetId::AsShared());
	}
};

class FOnlineSessionInfoSimplyStream : public FOnlineSessionInfo
{
protected:
	FOnlineSessionInfoSimplyStream(const FOnlineSessionInfoSimplyStream& Src) = delete;
	FOnlineSessionInfoSimplyStream& operator=(const FOnlineSessionInfoSimplyStream& Src) = delete;

public:
	TSharedPtr<class FInternetAddr> HostAddr;
	FUniqueNetIdPtr SessionId;

	FOnlineSessionInfoSimplyStream();
	explicit FOnlineSessionInfoSimplyStream(const FUniqueNetIdPtr& InSessionId);
	void Init();

	virtual ~FOnlineSessionInfoSimplyStream() = default;

	virtual const uint8* GetBytes() const override { return nullptr; }
	virtual int32 GetSize() const override { return 0; }
	virtual bool IsValid() const override { return SessionId.IsValid() && SessionId->IsValid(); }
	virtual FString ToString() const override { return SessionId.IsValid() ? SessionId->ToString() : TEXT(""); }
	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("Host: %s SessionId: %s"),
			HostAddr.IsValid() ? *HostAddr->ToString(true) : TEXT("INVALID"),
			SessionId.IsValid() ? *SessionId->ToDebugString() : TEXT("INVALID"));
	}
	virtual const FUniqueNetId& GetSessionId() const override { return *SessionId; }
};
