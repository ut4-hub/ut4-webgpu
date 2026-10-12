// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/SpscQueue.h"
#include "GenericPlatform/HttpRequestCommon.h"
#include "GenericPlatform/HttpResponseCommon.h"
#include "HAL/ThreadSafeBool.h"

class FSimplyStreamHttpResponse;
struct FetchData;

/**
 * SimplyStream implementation of an HTTP request
 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
class FSimplyStreamHttpRequest : public FHttpRequestCommon
PRAGMA_ENABLE_DEPRECATION_WARNINGS
{
public:

	// implementation friends
	friend class FSimplyStreamHttpResponse;

	//~ Begin IHttpBase Interface
	virtual const FString& GetURL() const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	virtual uint8_t* GetContentRaw() const { return RequestPayload.IsValid() ? (uint8_t*)RequestPayload->GetContent().GetData() : nullptr; }
	virtual const TMap<FString, FString>& GetHeaders() const { return Headers; }
	//~ End IHttpBase Interface

	//~ Begin IHttpRequest Interface
	virtual FString GetVerb() const override;
	virtual void SetVerb(const FString& InVerb) override;
	virtual void SetURL(const FString& InURL) override;
	virtual void SetOption(const FName Option, const FString& OptionValue) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContent(TArray<uint8>&& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
	virtual bool SetContentAsStreamedFile(const FString& Filename) override;
	virtual bool SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End IHttpRequest Interface

	//~ Begin IHttpRequestThreaded Interface
	virtual bool StartThreadedRequest() override;
	virtual void FinishRequest() override;
	virtual bool IsThreadedRequestComplete() override;
	virtual void TickThreadedRequest(float DeltaSeconds) override;
	//~ End IHttpRequestThreaded Interface

	/**
	 * @return true if the request was successfully setup
	 */
	bool SetupRequestHttpThread();

	/**
	 * Perform the http-thread cleanup of the request
	 */
	void  CleanupRequestHttpThread();

	/**
	 * Returns FetchData's handle - needed for HTTP manager.
	 */
	inline FetchData* GetFetchDataHandle() const
	{
		return Handle;
	}

	/**
	 * Marks request as completed (set by HTTP manager).
	 * @param CompletionResult Operation result code as returned by javascript
	 */
	void MarkAsCompleted(int InCompletionResult);

	FSimplyStreamHttpRequest();
	virtual ~FSimplyStreamHttpRequest();

	/**
	 * Method called when fetch wants us to supply more data
	 *
	 * @param Ptr buffer to copy data to
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @return number of bytes actually written to buffer
	 */
	size_t UploadCallback(void* Ptr, size_t SizeInBlocks);

	/**
	 * Headers will be passedline by line (i.e. this callback will be called with a full line), not necessarily zero-terminated.
	 * This callback will be also passed any intermediate headers, not only final response's ones.
	 *
	 * @param Ptr buffer to copy data to
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @return number of bytes actually processed, error is triggered if it does not match number of bytes passed
	 */
	size_t ReceiveResponseHeadersCallback(void* Ptr, size_t Size);


	/**
	 * Method called when fetch wants us to receive response body
	 *
	 * @param Ptr buffer to copy data to
	 * @param SizeInBlocks size of above buffer, in 'blocks'
	 * @param BlockSizeInBytes size of a single block
	 * @return number of bytes actually processed, error is triggered if it does not match number of bytes passed
	 */
	size_t ReceiveResponseBodyCallback(void* Ptr, size_t Size);
private:

	/**
	 * Perform the game-thread setup of the request
	 * @return true if the request was successfully setup
	 */
	virtual bool SetupRequest() override;

	virtual void AbortRequest() override;

	/**
	 * Trigger the request progress delegate if progress has changed
	 */
	void CheckProgressDelegate();

	/** Broadcast newly received headers */
	void BroadcastNewlyReceivedHeaders();

	/** Combine a header's key/value in the format "Key: Value" */
	static FString CombineHeaderKeyValue(const FString& HeaderKey, const FString& HeaderValue);

	virtual void CleanupRequest() override;

	void OnAnyActivityOccur(FStringView Reason);

	virtual void ClearInCaseOfRetry() override;

	virtual FHttpResponsePtr CreateResponse() override;
	virtual void MockResponseData() override;

private:
	FetchData*		Handle;
	FString			URL;
	FString			Verb;
	std::atomic<bool> bSimplyStreamRequestCompleted;
	/** Set to true when request has "30* Multiple Choices" (e.g. 301 Moved Permanently, 302 temporary redirect, 308 Permanent Redirect, etc.) */
	bool			bRedirected;
	/** Operation result code as returned by fetch */
	int		FetchCompletionResult; // 0 - success, 1 - error
	/** Mapping of header section to values. */
	TMap<FString, FString> Headers;
	/** Have we had any HTTP activity with the host? Sending headers, SSL handshake, etc */
	bool bAnyHttpActivity;
	/** Newly received headers we need to inform listeners about */
	TSpscQueue<TPair<FString, FString>> NewlyReceivedHeaders;
	/** Number of bytes sent already */
	std::atomic<int64> BytesSent;
	/** Total number of bytes sent already (includes data re-sent by seek attempts) */
	std::atomic<int64> TotalBytesSent;
	/** Caches how many bytes of the response we've read so far */
	std::atomic<int64> TotalBytesRead;
	/** Last bytes read reported to progress delegate */
	uint64 LastReportedBytesRead;
	/** Last bytes sent reported to progress delegate */
	uint64 LastReportedBytesSent;
};

/**
 * SimplyStream implementation of an HTTP response
 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
class FSimplyStreamHttpResponse : public FHttpResponseCommon
PRAGMA_ENABLE_DEPRECATION_WARNINGS
{
public:
	// implementation friends
	friend class FSimplyStreamHttpRequest;

	//~ Begin IHttpBase Interface
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	//~ End IHttpBase Interface

	//~ Begin IHttpResponse Interface
	virtual FString GetContentAsString() const override;
	//~ End IHttpResponse Interface

	/**
	 * @param InRequest - original request that created this response
	 */
	FSimplyStreamHttpResponse(const FSimplyStreamHttpRequest& InRequest);

private:
	/** BYTE array to fill in as the response is read via didReceiveData */
	TArray<uint8> Payload;
	/** The stream to receive response body */
	TSharedPtr<FArchive> ResponseBodyReceiveStream;
	/** Cached key/value header pairs. Parsed once request completes. Only accessible on the game thread. */
	TMap<FString, FString> Headers;
	/** Cached content length from completed response */
	uint64 ContentLength;
	/** True when the response has finished async processing */
	int32 volatile bIsReady;
	int32 volatile bSucceeded;
};