// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "OnlineSubsystemSimplyStreamTypes.h"

class FOnlineSubsystemSimplyStream;

/**
 * Implementation of IOnlinePurchase for SimplyStream platform with crypto and NFT support
 */
class ONLINESUBSYSTEMSIMPLYSTREAM_API FOnlinePurchaseSimplyStream : public IOnlinePurchase, public TSharedFromThis<FOnlinePurchaseSimplyStream, ESPMode::ThreadSafe>
{
public:
    FOnlinePurchaseSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem);
    virtual ~FOnlinePurchaseSimplyStream() = default;

    // IOnlinePurchase interface
    virtual bool IsAllowedToPurchase(const FUniqueNetId& UserId) override;
    virtual bool IsAllowedToPurchase(const FUniqueNetId& UserId, FOnlineError& Error) override;
    virtual void Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate) override;
    virtual void Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseReceiptlessCheckoutComplete& Delegate) override;
    virtual void FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId) override;
    virtual void RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate) override;
    virtual void QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate) override;
    virtual void GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const override;
    virtual void FinalizeReceiptValidationInfo(const FUniqueNetId& UserId, FString& InReceiptValidationInfo, const FOnFinalizeReceiptValidationInfoComplete& Delegate) override;

    // SimplyStream-specific crypto/NFT purchase methods
    void CheckoutWithCrypto(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FString& CryptoCurrency, const FString& WalletAddress, const FOnPurchaseCheckoutComplete& Delegate);
    void ConfirmCryptoPurchase(const FUniqueNetId& UserId, const FString& PurchaseId, const FString& TransactionHash, const FOnPurchaseCheckoutComplete& Delegate);

private:
    /** Reference to the main subsystem */
    FOnlineSubsystemSimplyStream* Subsystem;

    /** Purchase receipts cache */
    TMap<FUniqueNetIdRef, TArray<FPurchaseReceipt>> PurchaseReceipts;

    /** Pending purchases */
    TMap<FString, FPurchaseCheckoutRequest> PendingPurchases;

    /** Only allow one checkout per user at a time */
    TSet<FUniqueNetIdRef> UsersInCheckout;

    /** HTTP completion handlers */
    void OnPurchaseInitiateComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FPurchaseCheckoutRequest CheckoutRequest, FOnPurchaseCheckoutComplete Delegate);
    void OnPurchaseConfirmComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString PurchaseId, FOnPurchaseCheckoutComplete Delegate);
    void OnQueryReceiptsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FOnQueryReceiptsComplete Delegate);

    /** Helper methods */
    TSharedRef<FPurchaseReceipt> CreateReceiptFromJson(const TSharedPtr<FJsonObject>& JsonReceipt, const FUniqueNetId& UserId) const;
    void CacheReceipts(const FUniqueNetIdRef& UserId, const TArray<FPurchaseReceipt>& Receipts);
    bool IsUserInCheckout(const FUniqueNetId& UserId) const;
    void SetUserCheckoutState(const FUniqueNetId& UserId, bool bInCheckout);
};

typedef TSharedPtr<FOnlinePurchaseSimplyStream, ESPMode::ThreadSafe> FOnlinePurchaseSimplyStreamPtr;