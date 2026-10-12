// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineStoreSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineIdentitySimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "GenericPlatform/GenericPlatformHttp.h"

FOnlineStoreV2SimplyStream::FOnlineStoreV2SimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
    : Subsystem(InSubsystem)
{
}

void FOnlineStoreV2SimplyStream::QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
    
    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryCategories: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(false, TEXT("User not logged in"));
        });
        return;
    }

    const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryCategories: No auth token"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(false, TEXT("No auth token"));
        });
        return;
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/store/products"));
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineStoreV2SimplyStream::OnQueryCategoriesComplete, UserId.AsShared(), Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryCategories: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(false, TEXT("Failed to send request"));
        });
    }
}

void FOnlineStoreV2SimplyStream::OnQueryCategoriesComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FOnQueryOnlineStoreCategoriesComplete Delegate)
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
            bool bApiSuccess = JsonObject->GetBoolField(TEXT("success"));
            if (bApiSuccess)
            {
                CachedCategories.Empty();
                TSet<FString> CategorySet;

                const TArray<TSharedPtr<FJsonValue>>* ProductsArray;
                if (JsonObject->TryGetArrayField(TEXT("results"), ProductsArray))
                {
                    for (const TSharedPtr<FJsonValue>& ProductValue : *ProductsArray)
                    {
                        TSharedPtr<FJsonObject> ProductObject = ProductValue->AsObject();
                        if (ProductObject.IsValid())
                        {
                            FString Category = ProductObject->GetStringField(TEXT("category"));
                            FString Subcategory = ProductObject->GetStringField(TEXT("subcategory"));
                            
                            if (!Category.IsEmpty() && !CategorySet.Contains(Category))
                            {
                                FOnlineStoreCategory StoreCategory;
                                StoreCategory.Id = Category;
                                StoreCategory.Description = FText::FromString(Category);
                                
                                CachedCategories.Add(StoreCategory);
                                CategorySet.Add(Category);
                            }
                        }
                    }
                }

                bSuccess = true;
                UE_LOG_ONLINE_STOREV2(Verbose, TEXT("QueryCategories: Retrieved %d categories"), CachedCategories.Num());
            }
            else
            {
                ErrorStr = JsonObject->GetStringField(TEXT("error"));
                UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryCategories failed: %s"), *ErrorStr);
            }
        }
        else
        {
            ErrorStr = TEXT("Failed to parse response");
        }
    }
    else
    {
        ErrorStr = FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    Delegate.ExecuteIfBound(bSuccess, ErrorStr);
}

void FOnlineStoreV2SimplyStream::QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);

    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryOffersByFilter: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(false, TArray<FUniqueOfferId>(), TEXT("User not logged in"));
        });
        return;
    }

    const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryOffersByFilter: No auth token"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(false, TArray<FUniqueOfferId>(), TEXT("No auth token"));
        });
        return;
    }

    // Build URL with filter parameters
    FString URL = Subsystem->GetAPIBaseURL() + TEXT("/api/v1/multiplayer/store/products");
    TArray<FString> QueryParams;

    // Apply filter parameters (keywords expected as key=value or standalone flags)
    for (const FString& Keyword : Filter.Keywords)
    {
        FString Key;
        FString Value;
        bool bHasPair = Keyword.Split(TEXT("="), &Key, &Value);
        if (!bHasPair)
        {
            bHasPair = Keyword.Split(TEXT(":"), &Key, &Value);
        }

        if (!bHasPair)
        {
            Key = Keyword;
            Value.Reset();
        }

        Key.TrimStartAndEndInline();
        Value.TrimStartAndEndInline();

        if (Key.Equals(TEXT("category"), ESearchCase::IgnoreCase) && !Value.IsEmpty())
        {
            QueryParams.Add(FString::Printf(TEXT("category=%s"), *FGenericPlatformHttp::UrlEncode(Value)));
        }
        else if (Key.Equals(TEXT("productType"), ESearchCase::IgnoreCase) && !Value.IsEmpty())
        {
            QueryParams.Add(FString::Printf(TEXT("productType=%s"), *FGenericPlatformHttp::UrlEncode(Value)));
        }
        else if (Key.Equals(TEXT("featured"), ESearchCase::IgnoreCase))
        {
            QueryParams.Add(TEXT("featured=true"));
        }
        else if (Key.Equals(TEXT("onlyNfts"), ESearchCase::IgnoreCase))
        {
            QueryParams.Add(TEXT("onlyNfts=true"));
        }
    }

    if (QueryParams.Num() > 0)
    {
        URL += TEXT("?") + FString::Join(QueryParams, TEXT("&"));
    }

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(URL);
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineStoreV2SimplyStream::OnQueryOffersComplete, UserId.AsShared(), Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryOffersByFilter: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([Delegate]()
        {
            Delegate.ExecuteIfBound(false, TArray<FUniqueOfferId>(), TEXT("Failed to send request"));
        });
    }
}

void FOnlineStoreV2SimplyStream::QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate)
{
    if (OfferIds.Num() == 0)
    {
        Subsystem->ExecuteNextTick([Delegate, UserId = UserId.AsShared()]()
        {
            Delegate.ExecuteIfBound(true, TArray<FUniqueOfferId>(), TEXT(""));
        });
        return;
    }

    IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
    const int32 LocalUserNum = SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);

    if (!IdentityInt.IsValid() || LocalUserNum == INDEX_NONE || IdentityInt->GetLoginStatus(LocalUserNum) != ELoginStatus::LoggedIn)
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryOffersById: User not logged in"));
        Subsystem->ExecuteNextTick([Delegate, OfferIds]()
        {
            Delegate.ExecuteIfBound(false, OfferIds, TEXT("User not logged in"));
        });
        return;
    }

    const FString AuthToken = IdentityInt->GetAuthToken(LocalUserNum);
    if (AuthToken.IsEmpty())
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryOffersById: No auth token"));
        Subsystem->ExecuteNextTick([Delegate, OfferIds]()
        {
            Delegate.ExecuteIfBound(false, OfferIds, TEXT("No auth token"));
        });
        return;
    }

    // For specific offer IDs, query individual products
    // In a real implementation, we might want a bulk endpoint
    FString OfferId = OfferIds[0]; // Query first offer as example
    FString URL = Subsystem->GetAPIBaseURL() + FString::Printf(TEXT("/api/v1/multiplayer/store/products/%s"), *OfferId);

    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(URL);
    HttpRequest->SetVerb(TEXT("GET"));
    HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineStoreV2SimplyStream::OnQueryOffersComplete, UserId.AsShared(), Delegate);
    
    if (!HttpRequest->ProcessRequest())
    {
        UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryOffersById: Failed to send HTTP request"));
        Subsystem->ExecuteNextTick([Delegate, OfferIds]()
        {
            Delegate.ExecuteIfBound(false, OfferIds, TEXT("Failed to send request"));
        });
    }
}

void FOnlineStoreV2SimplyStream::OnQueryOffersComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef UserId, FOnQueryOnlineStoreOffersComplete Delegate)
{
    bool bSuccess = false;
    FString ErrorStr;
    TArray<FUniqueOfferId> QueriedOfferIds;

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
                // Handle array response (products list)
                const TArray<TSharedPtr<FJsonValue>>* ProductsArray;
                if (JsonObject->TryGetArrayField(TEXT("results"), ProductsArray))
                {
                    for (const TSharedPtr<FJsonValue>& ProductValue : *ProductsArray)
                    {
                        TSharedPtr<FJsonObject> ProductObject = ProductValue->AsObject();
                        if (ProductObject.IsValid())
                        {
                            FOnlineStoreOfferPtr Offer = CreateOfferFromJson(ProductObject);
                            if (Offer.IsValid())
                            {
                                CacheOffer(Offer);
                                QueriedOfferIds.Add(Offer->OfferId);
                            }
                        }
                    }
                }
                // Handle single product response
                else if (JsonObject->HasField(TEXT("result")))
                {
                    TSharedPtr<FJsonObject> ProductObject = JsonObject->GetObjectField(TEXT("result"));
                    if (ProductObject.IsValid())
                    {
                        FOnlineStoreOfferPtr Offer = CreateOfferFromJson(ProductObject);
                        if (Offer.IsValid())
                        {
                            CacheOffer(Offer);
                            QueriedOfferIds.Add(Offer->OfferId);
                        }
                    }
                }

                bSuccess = true;
                UE_LOG_ONLINE_STOREV2(Verbose, TEXT("QueryOffers: Retrieved %d offers"), QueriedOfferIds.Num());
            }
            else
            {
                ErrorStr = JsonObject->GetStringField(TEXT("error"));
                UE_LOG_ONLINE_STOREV2(Warning, TEXT("QueryOffers failed: %s"), *ErrorStr);
            }
        }
        else
        {
            ErrorStr = TEXT("Failed to parse response");
        }
    }
    else
    {
        ErrorStr = FString::Printf(TEXT("HTTP error %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    Delegate.ExecuteIfBound(bSuccess, QueriedOfferIds, ErrorStr);
}

void FOnlineStoreV2SimplyStream::GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const
{
    OutCategories = CachedCategories;
}

void FOnlineStoreV2SimplyStream::GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const
{
    OutOffers.Reset(CachedOffers.Num());
    for (const auto& OfferPair : CachedOffers)
    {
        if (OfferPair.Value.IsValid())
        {
            OutOffers.Add(OfferPair.Value.ToSharedRef());
        }
    }
}

FOnlineStoreOfferPtr FOnlineStoreV2SimplyStream::GetOffer(const FUniqueOfferId& OfferId) const
{
    const FOnlineStoreOfferPtr* FoundOffer = CachedOffers.Find(OfferId);
    if (FoundOffer && FoundOffer->IsValid())
    {
        return *FoundOffer;
    }
    return nullptr;
}

FOnlineStoreOfferPtr FOnlineStoreV2SimplyStream::CreateOfferFromJson(const TSharedPtr<FJsonObject>& JsonOffer) const
{
    if (!JsonOffer.IsValid())
    {
        return nullptr;
    }

    FString ProductId = JsonOffer->GetStringField(TEXT("productId"));
    FString Name = JsonOffer->GetStringField(TEXT("name"));
    FString Description = JsonOffer->GetStringField(TEXT("description"));
    
    TSharedRef<FOnlineStoreOfferSimplyStream> Offer = MakeShared<FOnlineStoreOfferSimplyStream>(ProductId, Name, Description);
    
    // Set basic offer properties
    Offer->NumericPrice = JsonOffer->GetNumberField(TEXT("basePriceUsd")) * 100; // Convert to cents
    Offer->CurrencyCode = TEXT("USD");
    Offer->ReleaseDate = FDateTime::FromUnixTimestamp(JsonOffer->GetNumberField(TEXT("releaseDate")));
    
    // Set SimplyStream-specific properties
    Offer->bIsCryptoEnabled = JsonOffer->GetBoolField(TEXT("cryptoEnabled"));
    Offer->bIsNft = JsonOffer->GetBoolField(TEXT("isNft"));
    
    if (JsonOffer->HasField(TEXT("nftContractAddress")))
    {
        Offer->NftContractAddress = JsonOffer->GetStringField(TEXT("nftContractAddress"));
    }
    
    if (JsonOffer->HasField(TEXT("nftChain")))
    {
        Offer->NftChain = JsonOffer->GetStringField(TEXT("nftChain"));
    }
    
    Offer->NftMaxSupply = JsonOffer->GetIntegerField(TEXT("nftMaxSupply"));
    Offer->NftCurrentSupply = JsonOffer->GetIntegerField(TEXT("nftCurrentSupply"));
    
    // Parse supported currencies
    const TArray<TSharedPtr<FJsonValue>>* CurrenciesArray;
    if (JsonOffer->TryGetArrayField(TEXT("supportedCurrencies"), CurrenciesArray))
    {
        for (const TSharedPtr<FJsonValue>& CurrencyValue : *CurrenciesArray)
        {
            Offer->SupportedCurrencies.Add(CurrencyValue->AsString());
        }
    }
    
    // Parse current crypto prices if available
    TSharedPtr<FJsonObject> CurrentPrices = JsonOffer->GetObjectField(TEXT("currentPrices"));
    if (CurrentPrices.IsValid())
    {
        for (const auto& PricePair : CurrentPrices->Values)
        {
            Offer->CurrentCryptoPrices.Add(FString(PricePair.Key), PricePair.Value->AsNumber());
        }
    }

    return Offer;
}

void FOnlineStoreV2SimplyStream::CacheOffer(const FOnlineStoreOfferPtr& Offer)
{
    if (Offer.IsValid())
    {
        CachedOffers.Add(Offer->OfferId, Offer);
    }
}
