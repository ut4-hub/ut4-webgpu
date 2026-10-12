// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineEventsSimplyStream.h"
#include "OnlineSubsystemSimplyStream.h"
#include "OnlineSubsystemSimplyStreamHelpers.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Base64.h"

// FOnlineEventsInterfaceSimplyStream implementation

FOnlineEventsInterfaceSimplyStream::FOnlineEventsInterfaceSimplyStream(FOnlineSubsystemSimplyStream* InSubsystem)
	: Subsystem(InSubsystem)
{
}

bool FOnlineEventsInterfaceSimplyStream::TriggerEvent(const FUniqueNetId& PlayerId, const TCHAR* EventName, const FOnlineEventParms& Parms)
{
	if (!EventName || FCString::Strlen(EventName) == 0)
	{
		UE_LOG_ONLINE_EVENTS(Warning, TEXT("TriggerEvent: Empty event name"));
		return false;
	}

	// Get local user number for auth token lookup
	int32 LocalUserNum = GetLocalUserNum(PlayerId);
	if (LocalUserNum == INDEX_NONE)
	{
		UE_LOG_ONLINE_EVENTS(Warning, TEXT("TriggerEvent: Cannot find local user for player ID"));
		return false;
	}

	FString AuthToken = GetJWTToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_EVENTS(Warning, TEXT("TriggerEvent: No auth token for local user %d"), LocalUserNum);
		return false;
	}

	// Serialize event parameters to JSON
	FString ParametersJson = SerializeEventParms(Parms);

	// Build request body
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("eventName"), EventName);

	// Parse parameters JSON string into object
	TSharedPtr<FJsonObject> ParametersObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ParametersJson);
	if (FJsonSerializer::Deserialize(Reader, ParametersObject) && ParametersObject.IsValid())
	{
		JsonObject->SetObjectField(TEXT("parameters"), ParametersObject);
	}
	else
	{
		// If parsing failed, create empty object
		JsonObject->SetObjectField(TEXT("parameters"), MakeShared<FJsonObject>());
	}

	// Add current timestamp
	JsonObject->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	// Build request URL
	FString URL = FString::Printf(TEXT("%s/api/v1/multiplayer/player/events"), *GetAPIBaseURL());

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineEventsInterfaceSimplyStream::OnTriggerEventComplete, FString(EventName));

	UE_LOG_ONLINE_EVENTS(Verbose, TEXT("TriggerEvent: Sending event '%s' with %d parameters"), EventName, Parms.Num());

	return HttpRequest->ProcessRequest();
}

void FOnlineEventsInterfaceSimplyStream::OnTriggerEventComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString EventName)
{
	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		UE_LOG_ONLINE_EVENTS(Verbose, TEXT("TriggerEvent: Event '%s' recorded successfully"), *EventName);
	}
	else
	{
		int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		UE_LOG_ONLINE_EVENTS(Warning, TEXT("TriggerEvent: Failed to record event '%s' (HTTP %d)"), *EventName, ResponseCode);
	}
}

void FOnlineEventsInterfaceSimplyStream::SetPlayerSessionId(const FUniqueNetId& PlayerId, const FGuid& PlayerSessionId)
{
	// Store session ID in memory
	{
		FScopeLock Lock(&SessionIdsLock);
		PlayerSessionIds.Add(PlayerId.AsShared(), PlayerSessionId);
	}

	// Get local user number for auth token lookup
	int32 LocalUserNum = GetLocalUserNum(PlayerId);
	if (LocalUserNum == INDEX_NONE)
	{
		UE_LOG_ONLINE_EVENTS(Warning, TEXT("SetPlayerSessionId: Cannot find local user for player ID"));
		return;
	}

	FString AuthToken = GetJWTToken(LocalUserNum);
	if (AuthToken.IsEmpty())
	{
		UE_LOG_ONLINE_EVENTS(Warning, TEXT("SetPlayerSessionId: No auth token for local user %d"), LocalUserNum);
		return;
	}

	// Build request body
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("playerSessionId"), PlayerSessionId.ToString(EGuidFormats::DigitsWithHyphens));

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	// Build request URL
	FString URL = FString::Printf(TEXT("%s/api/v1/multiplayer/player/events/session"), *GetAPIBaseURL());

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(URL);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AuthToken));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(this, &FOnlineEventsInterfaceSimplyStream::OnSetPlayerSessionIdComplete, PlayerId.AsShared(), PlayerSessionId);

	UE_LOG_ONLINE_EVENTS(Verbose, TEXT("SetPlayerSessionId: Setting session ID %s for player"), *PlayerSessionId.ToString());

	HttpRequest->ProcessRequest();
}

void FOnlineEventsInterfaceSimplyStream::OnSetPlayerSessionIdComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FUniqueNetIdRef PlayerId, FGuid SessionId)
{
	if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		UE_LOG_ONLINE_EVENTS(Verbose, TEXT("SetPlayerSessionId: Session ID %s set successfully"), *SessionId.ToString());
	}
	else
	{
		int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		UE_LOG_ONLINE_EVENTS(Warning, TEXT("SetPlayerSessionId: Failed to set session ID (HTTP %d)"), ResponseCode);
	}
}

FString FOnlineEventsInterfaceSimplyStream::SerializeEventParms(const FOnlineEventParms& Parms)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	for (const auto& Pair : Parms)
	{
		const FString& Key = Pair.Key;
		const FVariantData& Value = Pair.Value;

		// Serialize based on variant type
		switch (Value.GetType())
		{
		case EOnlineKeyValuePairDataType::Int32:
			{
				int32 IntValue;
				Value.GetValue(IntValue);
				JsonObject->SetNumberField(Key, IntValue);
			}
			break;

		case EOnlineKeyValuePairDataType::UInt32:
			{
				uint32 UIntValue;
				Value.GetValue(UIntValue);
				JsonObject->SetNumberField(Key, UIntValue);
			}
			break;

		case EOnlineKeyValuePairDataType::Int64:
			{
				int64 Int64Value;
				Value.GetValue(Int64Value);
				JsonObject->SetNumberField(Key, Int64Value);
			}
			break;

		case EOnlineKeyValuePairDataType::UInt64:
			{
				uint64 UInt64Value;
				Value.GetValue(UInt64Value);
				JsonObject->SetNumberField(Key, static_cast<double>(UInt64Value));
			}
			break;

		case EOnlineKeyValuePairDataType::Double:
			{
				double DoubleValue;
				Value.GetValue(DoubleValue);
				JsonObject->SetNumberField(Key, DoubleValue);
			}
			break;

		case EOnlineKeyValuePairDataType::Float:
			{
				float FloatValue;
				Value.GetValue(FloatValue);
				JsonObject->SetNumberField(Key, FloatValue);
			}
			break;

		case EOnlineKeyValuePairDataType::String:
			{
				FString StringValue;
				Value.GetValue(StringValue);
				JsonObject->SetStringField(Key, StringValue);
			}
			break;

		case EOnlineKeyValuePairDataType::Bool:
			{
				bool BoolValue;
				Value.GetValue(BoolValue);
				JsonObject->SetBoolField(Key, BoolValue);
			}
			break;

		case EOnlineKeyValuePairDataType::Blob:
			{
				TArray<uint8> BlobValue;
				Value.GetValue(BlobValue);
				// Encode binary data as base64 string
				FString Base64String = FBase64::Encode(BlobValue.GetData(), BlobValue.Num());
				JsonObject->SetStringField(Key, Base64String);
			}
			break;

		case EOnlineKeyValuePairDataType::Json:
			{
				FString JsonString;
				Value.GetValue(JsonString);
				// Parse the JSON string and embed as object
				TSharedPtr<FJsonValue> JsonValue;
				TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
				if (FJsonSerializer::Deserialize(JsonReader, JsonValue) && JsonValue.IsValid())
				{
					JsonObject->SetField(Key, JsonValue);
				}
				else
				{
					// If parsing fails, store as string
					JsonObject->SetStringField(Key, JsonString);
				}
			}
			break;

		default:
			// Empty or unknown type - skip
			UE_LOG_ONLINE_EVENTS(Verbose, TEXT("SerializeEventParms: Skipping parameter '%s' with empty/unknown type"), *Key);
			break;
		}
	}

	// Serialize to JSON string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}

FString FOnlineEventsInterfaceSimplyStream::GetAPIBaseURL() const
{
	return Subsystem->GetAPIBaseURL();
}

FString FOnlineEventsInterfaceSimplyStream::GetJWTToken(int32 LocalUserNum) const
{
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
		return IdentityInt->GetAuthToken(LocalUserNum);
	}
	return FString();
}

int32 FOnlineEventsInterfaceSimplyStream::GetLocalUserNum(const FUniqueNetId& UserId) const
{
	IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
		return SimplyStreamIdentity::GetLocalUserNumFromUniqueNetId(IdentityInt, UserId);
	}
	return INDEX_NONE;
}
