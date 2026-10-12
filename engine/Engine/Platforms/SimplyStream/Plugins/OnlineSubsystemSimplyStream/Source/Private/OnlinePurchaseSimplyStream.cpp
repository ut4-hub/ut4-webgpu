// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlinePurchaseSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#ifndef SIMPLYSTREAM_ENABLE_PURCHASES
#define SIMPLYSTREAM_ENABLE_PURCHASES 0
#endif

#if SIMPLYSTREAM_ENABLE_PURCHASES
#include "Serialization/JsonWriter.h"

FOnlinePurchaseSimplyStream::FOnlinePurchaseSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

bool FOnlinePurchaseSimplyStream::IsAllowedToPurchase(const FUniqueNetId& UserId, FOnlineError& Error)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    if (!IdentityInt.IsValid())
    {
        return false;
    }
    
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    return LocalUserNum != INDEX_NONE && IdentityInt->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn;
}

bool FOnlinePurchaseSimplyStream::IsAllowedToPurchase(const FUniqueNetId& UserId)
{
    FOnlineError Error;
    return IsAllowedToPurchase(UserId, Error);
}

void FOnlinePurchaseSimplyStream::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
    FOnlineError Result(true);
    
    // Validate user
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    if (!IdentityInt.IsValid())
    {
        Result.SetFromErrorCode(TEXT("com.simplystream.purchase.missing_identity"));
    }
    else if (LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        Result.SetFromErrorCode(TEXT("com.simplystream.purchase.invalid_user"));
    }

    // Validate request
    if (Result.WasSuccessful())
    {
        if (CheckoutRequest.PurchaseOffers.Num() <= 0)
        {
            Result.SetFromErrorCode(TEXT("com.simplystream.purchase.no_offers"));
        }
    }

    // Check if user is already in checkout
    if (Result.WasSuccessful())
    {
        if (IsUserInCheckout(UserId))
        {
            Result.SetFromErrorCode(TEXT("com.simplystream.purchase.already_in_checkout"));
        }
    }

    if (Result.WasSuccessful())
    {
        SetUserCheckoutState(UserId, true);
        
        const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, UserId);
        if (AuthToken.IsEmpty())
        {
            Result.SetFromErrorCode(TEXT("com.simplystream.purchase.no_auth_token"));
            SetUserCheckoutState(UserId, false);
        }
        else
        {
            // For now, process first offer only (multi-offer purchases would need separate implementation)
            const FPurchaseCheckoutRequest::FPurchaseOfferEntry& OfferEntry = CheckoutRequest.PurchaseOffers[0];
            
            // Build request JSON
            TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
            RequestJson->SetStringField(TEXT("productId"), OfferEntry.OfferId);
            RequestJson->SetNumberField(TEXT("quantity"), OfferEntry.Quantity);
            RequestJson->SetStringField(TEXT("paymentMethod"), TEXT("stripe")); // Default to traditional payment
            RequestJson->SetStringField(TEXT("paymentCurrency"), TEXT("USD"));

            FString RequestContent;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
            FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

            TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
            HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/store/purchase/initiate"));
            HttpRequest->SetVerb(TEXT("POST"));
            HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
            HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
            HttpRequest->SetContentAsString(RequestContent);

            HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlinePurchaseSimplyStream::OnPurchaseInitiateComplete, UserId.AsShared(), CheckoutRequest, Delegate);
            
            if (!HttpRequest->ProcessRequest())
            {
                Result.SetFromErrorCode(TEXT("com.simplystream.purchase.request_failed"));
                SetUserCheckoutState(UserId, false);
            }
        }
    }

    if (!Result.WasSuccessful())
    {
        UE_LOG_ONLINE_PURCHASE(Warning, TEXT("Checkout: Failed with Result=[%s]"), *Result.ToLogString());
        Subsystem->ExecuteNextTick([Result = MoveTemp(Result), Delegate]()
        {
            static const TSharedRef<FPurchaseReceipt> BlankReceipt(MakeShared<FPurchaseReceipt>());
            Delegate.ExecuteIfBound(Result, BlankReceipt);
        });
    }
}

void FOnlinePurchaseSimplyStream::OnPurchaseInitiateComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FPurchaseCheckoutRequest CheckoutRequest, FOnPurchaseCheckoutComplete Delegate)
{
    FOnlineError Result(false);
    TSharedRef<FPurchaseReceipt> Receipt = MakeShared<FPurchaseReceipt>();
    
    SetUserCheckoutState(*UserId, false);

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bApiSuccess)
            {
                TSharedPtr<FJsonObject> ResultObject = JsonObject->GetObjectField(TEXT("result"));
                if (ResultObject.IsValid())
                {
                    FString PurchaseId = ResultObject->GetStringField(TEXT("purchaseId"));
                    FString Status = ResultObject->GetStringField(TEXT("status"));
                    
                    // Store pending purchase for potential confirmation
                    PendingPurchases.Add(PurchaseId, CheckoutRequest);
                    
                    // Create receipt
                    Receipt->TransactionId = PurchaseId;
                    Receipt->ReceiptId = PurchaseId;
                    Receipt->TransactionState = Status == TEXT("pending") ? EPurchaseTransactionState::Purchasing : EPurchaseTransactionState::Purchased;
                    
                    // Add receipt offers
                    for (const auto& OfferEntry : CheckoutRequest.PurchaseOffers)
                    {
                        FPurchaseReceipt::FReceiptOfferEntry ReceiptOffer;
                        ReceiptOffer.OfferId = OfferEntry.OfferId;
                        ReceiptOffer.Quantity = OfferEntry.Quantity;
                        ReceiptOffer.LineItems.Add(FPurchaseReceipt::FLineItemInfo(OfferEntry.OfferId, OfferEntry.OfferId));
                        Receipt->AddReceiptOffer(ReceiptOffer);
                    }

                    Result = FOnlineError(true);
                    UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("Purchase initiated successfully: %s"), *PurchaseId);
                }
            }
            else
            {
                FString ErrorMsg = JsonObject->GetStringField(TEXT("error"));
                Result.SetFromErrorCode(FString::Printf(TEXT("com.simplystream.purchase.api_error: %s"), *ErrorMsg));
            }
        }
    }
    else
    {
        Result.SetFromErrorCode(FString::Printf(TEXT("com.simplystream.purchase.http_error: %d"), Response.IsValid() ? Response->GetResponseCode() : 0));
    }

    Delegate.ExecuteIfBound(Result, Receipt);
}

void FOnlinePurchaseSimplyStream::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseReceiptlessCheckoutComplete& Delegate)
{
    // Forward to main checkout method, ignoring receipt
    Checkout(UserId, CheckoutRequest, FOnPurchaseCheckoutComplete::CreateLambda([Delegate](const FOnlineError& Error, const TSharedRef<FPurchaseReceipt>&)
    {
        Delegate.ExecuteIfBound(Error);
    }));
}

void FOnlinePurchaseSimplyStream::FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId)
{
    // Crypto purchases might need confirmation - delegate to ConfirmCryptoPurchase if needed
    UE_LOG_ONLINE_PURCHASE(Log, TEXT("FinalizePurchase called for receipt: %s"), *ReceiptId);
}

void FOnlinePurchaseSimplyStream::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
    // Not implemented for SimplyStream
    Subsystem->ExecuteNextTick([Delegate]()
    {
        static const FOnlineError NotImplementedResult(EOnlineErrorResult::NotImplemented);
        static const TSharedRef<FPurchaseReceipt> BlankReceipt(MakeShared<FPurchaseReceipt>());
        Delegate.ExecuteIfBound(NotImplementedResult, BlankReceipt);
    });
}

void FOnlinePurchaseSimplyStream::QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE)
    {
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::InvalidAuth));
        });
        return;
    }

    if (IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::NotLoggedIn));
        });
        return;
    }

    const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, UserId);
    if (AuthToken.IsEmpty())
    {
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::InvalidAuth));
        });
        return;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/store/inventory"));
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlinePurchaseSimplyStream::OnQueryReceiptsComplete, UserId.AsShared(), Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::RequestFailure));
        });
    }
}

void FOnlinePurchaseSimplyStream::OnQueryReceiptsComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FOnQueryReceiptsComplete Delegate)
{
    FOnlineError Result(false);

    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bApiSuccess)
            {
                TArray<FPurchaseReceipt> Receipts;
                const TArray<TSharedPtr<FJsonValue>>* InventoryArray;
                if (JsonObject->TryGetArrayField(TEXT("results"), InventoryArray))
                {
                    for (const TSharedPtr<FJsonValue>& InventoryValue : *InventoryArray)
                    {
                        TSharedPtr<FJsonObject> InventoryObject = InventoryValue->AsObject();
                        if (InventoryObject.IsValid())
                        {
                            TSharedRef<FPurchaseReceipt> Receipt = CreateReceiptFromJson(InventoryObject, *UserId);
                            Receipts.Add(*Receipt);
                        }
                    }
                }
                
                CacheReceipts(UserId, Receipts);
                Result = FOnlineError(true);
            }
        }
    }

    Delegate.ExecuteIfBound(Result);
}

void FOnlinePurchaseSimplyStream::GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const
{
    const TArray<FPurchaseReceipt>* CachedReceipts = PurchaseReceipts.Find(UserId.AsShared());
    if (CachedReceipts)
    {
        OutReceipts = *CachedReceipts;
    }
    else
    {
        OutReceipts.Empty();
    }
}

void FOnlinePurchaseSimplyStream::FinalizeReceiptValidationInfo(const FUniqueNetId& UserId, FString& InReceiptValidationInfo, const FOnFinalizeReceiptValidationInfoComplete& Delegate)
{
    FOnlineError DefaultSuccess(true);
    Delegate.ExecuteIfBound(DefaultSuccess, InReceiptValidationInfo);
}

void FOnlinePurchaseSimplyStream::CheckoutWithCrypto(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FString& CryptoCurrency, const FString& WalletAddress, const FOnPurchaseCheckoutComplete& Delegate)
{
    // Similar to regular checkout but specify crypto payment method
    // Implementation would be similar to regular Checkout but with crypto-specific parameters
    UE_LOG_ONLINE_PURCHASE(Log, TEXT("CheckoutWithCrypto: %s via %s"), *CheckoutRequest.PurchaseOffers[0].OfferId, *CryptoCurrency);
    
    // For now, delegate to regular checkout - full crypto implementation would extend this
    Checkout(UserId, CheckoutRequest, Delegate);
}

void FOnlinePurchaseSimplyStream::ConfirmCryptoPurchase(const FUniqueNetId& UserId, const FString& PurchaseId, const FString& TransactionHash, const FOnPurchaseCheckoutComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const FString AuthToken = SimplyStreamIdentity::GetAuthTokenForUser(IdentityInt, UserId);
    if (!IdentityInt.IsValid() || AuthToken.IsEmpty())
    {
        Subsystem->ExecuteNextTick([Delegate]()
        {
            static const TSharedRef<FPurchaseReceipt> BlankReceipt(MakeShared<FPurchaseReceipt>());
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::InvalidAuth), BlankReceipt);
        });
        return;
    }
    
    // Build confirmation request
    TSharedPtr<FJsonObject> RequestJson = MakeShared<FJsonObject>();
    RequestJson->SetStringField(TEXT("transactionHash"), TransactionHash);

    FString RequestContent;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestContent);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/store/purchase/%s/confirm"), *PurchaseId));
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetContentAsString(RequestContent);

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlinePurchaseSimplyStream::OnPurchaseConfirmComplete, UserId.AsShared(), PurchaseId, Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        Subsystem->ExecuteNextTick([Delegate]()
        {
            static const TSharedRef<FPurchaseReceipt> BlankReceipt(MakeShared<FPurchaseReceipt>());
            Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::RequestFailure), BlankReceipt);
        });
    }
}

void FOnlinePurchaseSimplyStream::OnPurchaseConfirmComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FString PurchaseId, FOnPurchaseCheckoutComplete Delegate)
{
    FOnlineError Result(false);
    TSharedRef<FPurchaseReceipt> Receipt = MakeShared<FPurchaseReceipt>();
    
    if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bApiSuccess)
            {
                // Purchase confirmed successfully
                Receipt->TransactionId = PurchaseId;
                Receipt->ReceiptId = PurchaseId;
                Receipt->TransactionState = EPurchaseTransactionState::Purchased;
                
                // Get pending purchase details if available
                if (PendingPurchases.Contains(PurchaseId))
                {
                    const FPurchaseCheckoutRequest& CheckoutRequest = PendingPurchases[PurchaseId];
                    for (const auto& OfferEntry : CheckoutRequest.PurchaseOffers)
                    {
                        FPurchaseReceipt::FReceiptOfferEntry ReceiptOffer;
                        ReceiptOffer.OfferId = OfferEntry.OfferId;
                        ReceiptOffer.Quantity = OfferEntry.Quantity;
                        ReceiptOffer.LineItems.Add(FPurchaseReceipt::FLineItemInfo(OfferEntry.OfferId, OfferEntry.OfferId));
                        Receipt->AddReceiptOffer(ReceiptOffer);
                    }
                    PendingPurchases.Remove(PurchaseId);
                }

                Result = FOnlineError(true);
                UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("Crypto purchase confirmed: %s"), *PurchaseId);
            }
        }
    }

    Delegate.ExecuteIfBound(Result, Receipt);
}

TSharedRef<FPurchaseReceipt> FOnlinePurchaseSimplyStream::CreateReceiptFromJson(const TSharedPtr<FJsonObject>& JsonReceipt, const FUniqueNetId& UserId) const
{
    TSharedRef<FPurchaseReceipt> Receipt = MakeShared<FPurchaseReceipt>();
    
    if (JsonReceipt.IsValid())
    {
        FString InventoryId = JsonReceipt->GetStringField(TEXT("inventoryId"));
        FString ProductId = JsonReceipt->GetStringField(TEXT("productId"));
        
        Receipt->TransactionId = InventoryId;
        Receipt->ReceiptId = InventoryId;
        Receipt->TransactionState = EPurchaseTransactionState::Purchased;
        
        FPurchaseReceipt::FReceiptOfferEntry ReceiptOffer;
        ReceiptOffer.OfferId = ProductId;
        ReceiptOffer.Quantity = JsonReceipt->GetIntegerField(TEXT("quantity"));
        ReceiptOffer.LineItems.Add(FPurchaseReceipt::FLineItemInfo(ProductId, ProductId));
        Receipt->AddReceiptOffer(ReceiptOffer);
    }
    
    return Receipt;
}

void FOnlinePurchaseSimplyStream::CacheReceipts(const FUniqueNetIdRef& UserId, const TArray<FPurchaseReceipt>& Receipts)
{
    PurchaseReceipts.Add(UserId, Receipts);
}

bool FOnlinePurchaseSimplyStream::IsUserInCheckout(const FUniqueNetId& UserId) const
{
    return UsersInCheckout.Contains(UserId.AsShared());
}

void FOnlinePurchaseSimplyStream::SetUserCheckoutState(const FUniqueNetId& UserId, bool bInCheckout)
{
    if (bInCheckout)
    {
        UsersInCheckout.Add(UserId.AsShared());
    }
    else
    {
        UsersInCheckout.Remove(UserId.AsShared());
    }
}

#else // SIMPLYSTREAM_ENABLE_PURCHASES

namespace
{
bool LogPurchasesDisabled(const TCHAR* MethodName)
{
    UE_LOG_ONLINE_PURCHASE(Verbose, TEXT("%s is disabled (SIMPLYSTREAM_ENABLE_PURCHASES=0)"), MethodName);
    return false;
}
}

FOnlinePurchaseSimplyStream::FOnlinePurchaseSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

bool FOnlinePurchaseSimplyStream::IsAllowedToPurchase(const FUniqueNetId& UserId, FOnlineError& Error)
{
    return false;
}

bool FOnlinePurchaseSimplyStream::IsAllowedToPurchase(const FUniqueNetId& UserId)
{
    FOnlineError Error;
    return IsAllowedToPurchase(UserId, Error);
}

void FOnlinePurchaseSimplyStream::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
    LogPurchasesDisabled(TEXT("Checkout"));
}

void FOnlinePurchaseSimplyStream::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseReceiptlessCheckoutComplete& Delegate)
{
    LogPurchasesDisabled(TEXT("CheckoutReceiptless"));
}

void FOnlinePurchaseSimplyStream::FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId)
{
    LogPurchasesDisabled(TEXT("FinalizePurchase"));
}

void FOnlinePurchaseSimplyStream::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
    LogPurchasesDisabled(TEXT("RedeemCode"));
}

void FOnlinePurchaseSimplyStream::QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate)
{
    LogPurchasesDisabled(TEXT("QueryReceipts"));
}

void FOnlinePurchaseSimplyStream::GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const
{
    OutReceipts.Reset();
}

void FOnlinePurchaseSimplyStream::FinalizeReceiptValidationInfo(const FUniqueNetId& UserId, FString& InReceiptValidationInfo, const FOnFinalizeReceiptValidationInfoComplete& Delegate)
{
    LogPurchasesDisabled(TEXT("FinalizeReceiptValidationInfo"));
}

void FOnlinePurchaseSimplyStream::CheckoutWithCrypto(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FString& CryptoCurrency, const FString& WalletAddress, const FOnPurchaseCheckoutComplete& Delegate)
{
    LogPurchasesDisabled(TEXT("CheckoutWithCrypto"));
}

void FOnlinePurchaseSimplyStream::ConfirmCryptoPurchase(const FUniqueNetId& UserId, const FString& PurchaseId, const FString& TransactionHash, const FOnPurchaseCheckoutComplete& Delegate)
{
    LogPurchasesDisabled(TEXT("ConfirmCryptoPurchase"));
}

#endif // SIMPLYSTREAM_ENABLE_PURCHASES
