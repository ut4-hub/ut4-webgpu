// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"
#include "OnlineSubsystemSimplyStreamTypes.h"

using FOnlineStoreOfferPtr = TSharedPtr<FOnlineStoreOffer>;

class FOnlineSubsystemSimplyStream;

/**
 * Store entry for SimplyStream platform
 */
class FOnlineStoreOfferSimplyStream : public FOnlineStoreOffer
{
public:
    FOnlineStoreOfferSimplyStream(const FString& InOfferId, const FString& InTitle, const FString& InDescription)
        : FOnlineStoreOffer()
    {
        OfferId = InOfferId;
        Title = FText::FromString(InTitle);
        Description = FText::FromString(InDescription);
    }

    virtual ~FOnlineStoreOfferSimplyStream() = default;

    // Additional SimplyStream-specific fields
    bool bIsCryptoEnabled = false;
    bool bIsNft = false;
    FString NftContractAddress;
    FString NftChain;
    int32 NftMaxSupply = 0;
    int32 NftCurrentSupply = 0;
    TArray<FString> SupportedCurrencies;
    TMap<FString, double> CurrentCryptoPrices;
};

/**
 * Interface for accessing the SimplyStream store
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlineStoreV2SimplyStream : public IOnlineStoreV2, public TSharedFromThis<FOnlineStoreV2SimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlineStoreV2SimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlineStoreV2SimplyStream() = default;

    // IOnlineStoreV2 interface
    virtual void QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate) override;
    virtual void QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate) override;
    virtual void QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate) override;
    virtual void GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const override;
    virtual void GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const override;
    virtual FOnlineStoreOfferPtr GetOffer(const FUniqueOfferId& OfferId) const override;

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* Subsystem;

    /** Cached store categories */
    TArray<FOnlineStoreCategory> CachedCategories;

    /** Cached store offers */
    TMap<FUniqueOfferId, FOnlineStoreOfferPtr> CachedOffers;

    /** HTTP completion handlers */
    void OnQueryCategoriesComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FOnQueryOnlineStoreCategoriesComplete Delegate);
    void OnQueryOffersComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FOnQueryOnlineStoreOffersComplete Delegate);

    /** Helper methods */
    FOnlineStoreOfferPtr CreateOfferFromJson(const TSharedPtr<FJsonObject>& JsonOffer) const;
    void CacheOffer(const FOnlineStoreOfferPtr& Offer);
};

typedef TSharedPtr<FOnlineStoreV2SimplyStream, ESPMode::ThreadSafe> FOnlineStoreV2SimplyStreamPtr;
