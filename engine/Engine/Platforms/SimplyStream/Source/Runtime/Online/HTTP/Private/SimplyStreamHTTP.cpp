// Copyright Epic Games, Inc. All Rights Reserved.
#include "SimplyStreamHTTP.h"
#include "Stats/Stats.h"
#include "Misc/App.h"
#include "HttpModule.h"
#include "Http.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "SimplyStreamHttpManager.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Regex.h"
#include "wonder_log.h"
#include "pthreadfs.h"

#define VERBOSE Verbose
// EMSCRIPTEN_STUFF_START
#define FETCH_OK 0
#define FETCH_ERROR 1
extern EmQueue fsQueue;
struct FetchData {
	FSimplyStreamHttpRequest* r = 0;
	char* url = 0;
	char* verb = 0;
	char* body = 0;
	const char** headers = 0;
	int32_t body_size = 0;
	std::vector<const char*> _headers;
	std::vector<std::string> _headers1;
	std::string _url;
	std::string _verb;
	FetchData(FSimplyStreamHttpRequest* _r) {
		r = _r;
	}
	// response stuff
	size_t ContentLength = 0;
	int HttpCode = 0;
	int GetHttpCode() {
		return HttpCode;
	}
	size_t GetContentLength() {
		return ContentLength;
	}
	void Init() {
		_headers.resize(r->GetHeaders().Num() * 2);
		_headers1.resize(r->GetHeaders().Num() * 2);
		_url = TCHAR_TO_UTF8(*r->GetURL());
		_verb = TCHAR_TO_UTF8(*r->GetVerb());
		url = (char*)_url.data();
		verb = (char*)_verb.data();
		body = (char*)r->GetContentRaw();
		body_size = r->GetContentLength();
		int i = 0;
		for (TMap<FString, FString>::TConstIterator It(r->GetHeaders()); It; ++It)
		{
			_headers1[i] = TCHAR_TO_UTF8(*It.Key());
			_headers1[i + 1] = TCHAR_TO_UTF8(*It.Value());
			_headers[i] = _headers1[i].data();
			_headers[i + 1] = _headers1[i + 1].data();
			i += 2;
		}
		_headers.push_back(nullptr);
		headers = _headers.data();
		//wonder_log::info("FetchData::FetchData() url={} verb={} body={} header_count:{} fd={} r={}", (const char*)url, (const char*)verb, (const char*)body, _headers.size(), (uint64_t)this, (uint64_t)r);
	}
	~FetchData() {
		//wonder_log::info("FetchData::~FetchData() url={} verb={} body={} header_count:{} fd={} r={}", (const char*)url, (const char*)verb, (const char*)body, _headers.size(), (uint64_t)this, (uint64_t)r);
	}
	void SetResponseData(const char* data, int size, int status, bool success) {
		ContentLength = size;
		HttpCode = status;
		r->ReceiveResponseBodyCallback((void*)data, size);
		//r->ReceiveResponseHeadersCallback((void*)header_data, header_size);
		r->MarkAsCompleted(success ? FETCH_OK : FETCH_ERROR);
	}
};
KEEP_IN_MODULE void downloadSucceeded(FetchData* fd, void* data, int size, int status) {
	//	strcpy(_headers[i], TCHAR_TO_ANSI(*headers[i]));
	fd->SetResponseData((const char*)data, size, status, true);
	free(data);
	//delete fd;
}
KEEP_IN_MODULE void downloadFailed(FetchData* fd, int status) {
	wonder_log::info("Download failed");
	fd->SetResponseData(0, 0, status, false);
	//delete fd;
}
extern "C" void UE_MakeHTTPDataRequest(FSimplyStreamHttpRequest* _this) {
	FetchData* fd = _this->GetFetchDataHandle();
	FS_QUEUE_ASM(fsQueue, {
		const fdPtr = Number($0);
		try {

		const sizeOfPointer = WNDR_JS_POINTER_SIZE();

		const _thisPtr = Number(getValue(fdPtr, '*'));
		const urlPtr = Number(getValue(fdPtr + sizeOfPointer, '*'));
		const verbPtr = Number(getValue(fdPtr + sizeOfPointer * 2, '*'));
		const bodyPtr = Number(getValue(fdPtr + sizeOfPointer * 3, '*'));
		let headersPtr = Number(getValue(fdPtr + sizeOfPointer * 4, '*'));
		let body_size = getValue(fdPtr + sizeOfPointer * 5, 'i32');

		const url = UTF8ToString(urlPtr);
		const verb = UTF8ToString(verbPtr);
		const bodyView = bodyPtr ? HEAPU8.subarray(bodyPtr, bodyPtr + body_size) : null;
		// Node's fetch cannot reliably consume a view backed by WASM shared memory.
		const body = ENVIRONMENT_IS_NODE && bodyView ? Uint8Array.from(bodyView) : bodyView;

		let headers = {};
		for (;;) {
			const key = Number(getValue(headersPtr, '*'));
			if (!key) break;
			const value = Number(getValue(headersPtr + sizeOfPointer, '*'));
			if (!value) break;
			headersPtr += sizeOfPointer * 2;
			const keyStr = UTF8ToString(key);
			const valueStr = UTF8ToString(value);
			headers[keyStr] = valueStr;
		}
		if (!url) {
			console.log('Missing url');
			_downloadFailed(WNDR_JS_POINTER(fdPtr), 0);
			return;
		}
		if (!verb) {
			console.log('Missing verb');
			_downloadFailed(WNDR_JS_POINTER(fdPtr), 0);
			return;
		}
		let r = await fetch(url, { method: verb, body: body, headers: headers });
		if (r.ok) {
			let buffer = await r.arrayBuffer();
			const data = new Uint8Array(buffer);
			const dataPtr = _malloc(data.length);
			HEAPU8.set(data, Number(dataPtr));
			_downloadSucceeded(WNDR_JS_POINTER(fdPtr), WNDR_JS_POINTER(dataPtr), data.length, r.status);
		}
		else {
			_downloadFailed(WNDR_JS_POINTER(fdPtr), r.status);
		}
		}
		 catch (e) {
			console.error('[wndr-http] fetch failed', e);
			_downloadFailed(WNDR_JS_POINTER(fdPtr), 0);
		  }
		}, fd);
}
// EMSCRIPTEN_STUFF_END
PRAGMA_DISABLE_DEPRECATION_WARNINGS
FSimplyStreamHttpRequest::FSimplyStreamHttpRequest()
	: Handle(nullptr)
	, Verb(TEXT("GET"))
	, bSimplyStreamRequestCompleted(false)
	, bRedirected(false)
	, bAnyHttpActivity(false)
	, BytesSent(0)
	, TotalBytesSent(0)
	, TotalBytesRead(0)
	, LastReportedBytesRead(0)
	, LastReportedBytesSent(0)
{
	checkf(FSimplyStreamHttpManager::IsInit(), TEXT("SimplyStream request was created before manager was initialized"));

	Handle = new FetchData(this);

	// Add default headers
	const TMap<FString, FString>& DefaultHeaders = FHttpModule::Get().GetDefaultHeaders();
	for (TMap<FString, FString>::TConstIterator It(DefaultHeaders); It; ++It)
	{
		SetHeader(It.Key(), It.Value());
	}

	bUsePlatformActivityTimeout = false;
}

FSimplyStreamHttpRequest::~FSimplyStreamHttpRequest()
{
	checkf(FSimplyStreamHttpManager::IsInit(), TEXT("SimplyStream request was held after the manager was shutdown."));
	if (Handle)
	{
		delete Handle;
		Handle = nullptr;
	}
}

const FString& FSimplyStreamHttpRequest::GetURL() const
{
	return URL;
}

FString FSimplyStreamHttpRequest::GetHeader(const FString& HeaderName) const
{
	FString Result;

	const FString* Header = Headers.Find(HeaderName);
	if (Header != NULL)
	{
		Result = *Header;
	}

	return Result;
}

FString FSimplyStreamHttpRequest::CombineHeaderKeyValue(const FString& HeaderKey, const FString& HeaderValue)
{
	FString Combined;
	const TCHAR Separator[] = TEXT(": ");
	constexpr const int32 SeparatorLength = UE_ARRAY_COUNT(Separator) - 1;
	Combined.Reserve(HeaderKey.Len() + SeparatorLength + HeaderValue.Len());
	Combined.Append(HeaderKey);
	Combined.AppendChars(Separator, SeparatorLength);
	Combined.Append(HeaderValue);
	return Combined;
}

TArray<FString> FSimplyStreamHttpRequest::GetAllHeaders() const
{
	TArray<FString> Result;
	Result.Reserve(Headers.Num());
	for (const TPair<FString, FString>& It : Headers)
	{
		Result.Emplace(CombineHeaderKeyValue(It.Key, It.Value));
	}
	return Result;
}

void FSimplyStreamHttpRequest::MockResponseData()
{
	FetchCompletionResult = FETCH_OK;
	bSimplyStreamRequestCompleted = true;

	TSharedPtr<FSimplyStreamHttpResponse> Response = StaticCastSharedPtr<FSimplyStreamHttpResponse>(ResponseCommon);
	Response->bSucceeded = true;
}


FString FSimplyStreamHttpRequest::GetContentType() const
{
	return GetHeader(TEXT( "Content-Type" ));
}

uint64 FSimplyStreamHttpRequest::GetContentLength() const
{
	return RequestPayload.IsValid() ? RequestPayload->GetContentLength() : 0;
}

const TArray<uint8>& FSimplyStreamHttpRequest::GetContent() const
{
	static const TArray<uint8> EmptyContent;
	return RequestPayload.IsValid() ? RequestPayload->GetContent() : EmptyContent;
}

void FSimplyStreamHttpRequest::SetVerb(const FString& InVerb)
{
	wonder_log::info("SetVerb {}", TCHAR_TO_UTF8(*InVerb));
	if (CompletionStatus == EHttpRequestStatus::Processing) {
		UE_LOG(LogHttp, Warning, TEXT("FSimplyStreamHttpRequest::SetVerb() - attempted to set verb on a request that is inflight"));
		return;
	}

	Verb = InVerb.ToUpper();
}

void FSimplyStreamHttpRequest::SetURL(const FString& InURL)
{
	if (CompletionStatus == EHttpRequestStatus::Processing) {
		UE_LOG(LogHttp, Warning, TEXT("FSimplyStreamHttpRequest::SetURL() - attempted to set url on a request that is inflight"));
		return;
	}

	URL = InURL;
}

void FSimplyStreamHttpRequest::SetOption(const FName Option, const FString& OptionValue)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FCurlHttpRequest::SetOption() - attempted to set option on a request that is inflight"));
		return;
	}

	FHttpRequestCommon::SetOption(Option, OptionValue);
}

void FSimplyStreamHttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	SetContent(CopyTemp(ContentPayload));
}

void FSimplyStreamHttpRequest::SetContent(TArray<uint8>&& ContentPayload)
{
	if (CompletionStatus == EHttpRequestStatus::Processing) {
		UE_LOG(LogHttp, Warning, TEXT("FSimplyStreamHttpRequest::SetContent() - attempted to set content on a request that is inflight"));
		return;
	}

	RequestPayload = MakeUnique<FRequestPayloadInMemory>(MoveTemp(ContentPayload));
}

void FSimplyStreamHttpRequest::SetContentAsString(const FString& ContentString)
{
	if (CompletionStatus == EHttpRequestStatus::Processing) {
		UE_LOG(LogHttp, Warning, TEXT("FSimplyStreamHttpRequest::SetContentAsString() - attempted to set content on a request that is inflight"));
		return;
	}

	uint64 Utf8Length = FPlatformString::ConvertedLength<UTF8CHAR>(*ContentString, ContentString.Len());
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(Utf8Length);
	FPlatformString::Convert((UTF8CHAR*)Buffer.GetData(), Buffer.Num(), *ContentString, ContentString.Len());
	RequestPayload = MakeUnique<FRequestPayloadInMemory>(MoveTemp(Buffer));
}

bool FSimplyStreamHttpRequest::SetContentAsStreamedFile(const FString& Filename)
{
	UE_LOG(LogHttp, Warning, TEXT("FSimplyStreamHttpRequest::SetContentAsStreamedFile is not implemented"));
	return false;
}

bool FSimplyStreamHttpRequest::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	UE_LOG(LogHttp, Warning, TEXT("FSimplyStreamHttpRequest::SetContentFromStream is not implemented"));
	return false;
}

void FSimplyStreamHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	if (CompletionStatus == EHttpRequestStatus::Processing) {
		UE_LOG(LogHttp, Warning, TEXT("FSimplyStreamHttpRequest::SetHeader() - attempted to set header on a request that is inflight"));
		return;
	}

	Headers.Add(HeaderName, HeaderValue);
}

void FSimplyStreamHttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (CompletionStatus == EHttpRequestStatus::Processing) {
		UE_LOG(LogHttp, Warning, TEXT("FSimplyStreamHttpRequest::AppendToHeader() - attempted to append to header on a request that is inflight"));
		return;
	}

	if (!HeaderName.IsEmpty() && !AdditionalHeaderValue.IsEmpty())
	{
		FString* PreviousValue = Headers.Find(HeaderName);
		FString NewValue;
		if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
		{
			NewValue = (*PreviousValue) + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

		SetHeader(HeaderName, NewValue);
	}
}

FString FSimplyStreamHttpRequest::GetVerb() const
{
	return Verb;
}

size_t FSimplyStreamHttpRequest::ReceiveResponseHeadersCallback(void* Ptr, size_t Size)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamHttpRequest_ReceiveResponseHeaderCallback);

	InitResponse();

	OnAnyActivityOccur(TEXTVIEW("Received headers"));
#if 0 // todo - headers
	FString Header(ANSI_TO_TCHAR(Ptr));
	Header = Header.Replace(TEXT("\r"), TEXT(""));
	FString HeaderChunk, HeaderLeftovers;
	while (Header.Split(TEXT("\n"), &HeaderChunk, &HeaderLeftovers))
	{
		if (!HeaderChunk.IsEmpty())
		{
			wonder_log::info("FSimplyStreamHttpRequest::ReceiveDataCallback() - {}", TCHAR_TO_UTF8(*HeaderChunk));

			FString HeaderKey, HeaderValue;
			if (HeaderChunk.Split(TEXT(":"), &HeaderKey, &HeaderValue))
			{
				FString* PreviousValue = Response->Headers.Find(HeaderKey);
				FString NewValue;
				if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
				{
					NewValue = (*PreviousValue) + TEXT(", ");
				}
				HeaderValue.TrimStartInline();
				NewValue += HeaderValue;
				Response->Headers.Add(HeaderKey, NewValue);
				if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread)
				{
					OnHeaderReceived().ExecuteIfBound(SharedThis(this), HeaderKey, HeaderValue);
				}
				else if (OnHeaderReceived().IsBound())
				{
					NewlyReceivedHeaders.Enqueue(TPair<FString, FString>(MoveTemp(HeaderKey), MoveTemp(HeaderValue)));
				}
			}
		}
		if (!HeaderLeftovers.IsEmpty())
		{
			Header = HeaderLeftovers;
		}
		else
		{
			break;
		}
	}
#endif

	return 0;
}

size_t FSimplyStreamHttpRequest::ReceiveResponseBodyCallback(void* Ptr, size_t SizeToDownload)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamHttpRequest_ReceiveResponseBodyCallback);
	LLM_SCOPE(ELLMTag::Networking);

	InitResponse();

	OnAnyActivityOccur(TEXTVIEW("Received body"));

	TSharedPtr<FSimplyStreamHttpResponse> Response = StaticCastSharedPtr<FSimplyStreamHttpResponse>(ResponseCommon);
	UE_LOG(LogHttp, VERBOSE, TEXT("%p: ReceiveResponseBodyCallback: %llu bytes out of %llu received. (TotalBytesRead=%llu, Response->GetContentLength()=%llu, SizeToDownload=%llu (<-this will get returned from the callback))"),
		this, TotalBytesRead.load() + SizeToDownload, Response->GetContentLength(),
		TotalBytesRead.load(), Response->GetContentLength(), SizeToDownload);

	// note that we can be passed 0 bytes if file transmitted has 0 length
	if (SizeToDownload == 0)
	{
		return 0;
	}

	Response->Payload.AddUninitialized(SizeToDownload);
	check(TotalBytesRead.load() + SizeToDownload <= Response->Payload.Num());
	FMemory::Memcpy(static_cast<uint8*>(Response->Payload.GetData()) + TotalBytesRead.load(), Ptr, SizeToDownload);

	TotalBytesRead += SizeToDownload;

	if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread)
	{
		CheckProgressDelegate();
	}

	return SizeToDownload;
}

size_t FSimplyStreamHttpRequest::UploadCallback(void* Ptr, size_t SizeInBlocks)
{
	OnAnyActivityOccur(TEXTVIEW("Upload callback"));

	size_t MaxBufferSize = SizeInBlocks;
	size_t SizeAlreadySent = BytesSent.load();
	size_t SizeSentThisTime = RequestPayload->FillOutputBuffer(Ptr, MaxBufferSize, SizeAlreadySent);
	BytesSent += SizeSentThisTime;
	TotalBytesSent += SizeSentThisTime;

	UE_LOG(LogHttp, VERBOSE, TEXT("%p: UploadCallback: %llu bytes out of %llu sent (%llu bytes total sent). (SizeInBlocks=%llu, SizeToSendThisTime=%llu (<-this will get returned from the callback))"),
		this, BytesSent.load(), RequestPayload->GetContentLength(), TotalBytesSent.load(), SizeInBlocks, SizeSentThisTime);

	return SizeSentThisTime;
}

void FSimplyStreamHttpRequest::OnAnyActivityOccur(FStringView Reason)
{
	if (!bAnyHttpActivity)
	{
		bAnyHttpActivity = true;

		ConnectTime = FPlatformTime::Seconds() - StartProcessTime;

		StartActivityTimeoutTimer();
	}

	ResetActivityTimeoutTimer(Reason);
}

void FSimplyStreamHttpRequest::CleanupRequestHttpThread()
{
}

bool FSimplyStreamHttpRequest::SetupRequest()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamHttpRequest_SetupRequest);
	check(Handle);

	if (!RequestPayload.IsValid())
	{
		RequestPayload = MakeUnique<FRequestPayloadInMemory>(TArray<uint8>());
	}

	if (!OpenRequestPayloadDefaultImpl())
	{
		return false;
	}

	bSimplyStreamRequestCompleted = false;
	LastReportedBytesSent = 0;

	UE_LOG(LogHttp, VERBOSE, TEXT("%p: Custom headers are %s"), this, Headers.Num() ? TEXT("present") : TEXT("NOT present"));
	UE_LOG(LogHttp, VERBOSE, TEXT("%p: Payload size=%llu"), this, RequestPayload->GetContentLength());

	if (GetHeader(TEXT("User-Agent")).IsEmpty())
	{
		//FString tmp = FPlatformHttp::GetDefaultUserAgent();
		//std::string tmp2 = TCHAR_TO_UTF8(*tmp);
		//wonder_log::info("setting user-agent {}", tmp2);
		SetHeader(TEXT("User-Agent"), TEXT("SimplyStream UE5.4"));
	}

	// content-length should be present http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.4
	if (GetHeader(TEXT("Content-Length")).IsEmpty())
	{
		SetHeader(TEXT("Content-Length"), FString::Printf(TEXT("%llu"), RequestPayload->GetContentLength()));
	}

	return true;
}

bool FSimplyStreamHttpRequest::SetupRequestHttpThread()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamHttpRequest_SetupRequestHttpThread);
	int safety = 100;
	while (Verb == "" && safety-- > 0) {
		wonder_log::info("Waiting on verb");
	}
	// set up verb (note that Verb is expected to be uppercase only)
	if (Verb == TEXT("POST"))
	{
		// If we don't pass any other Content-Type, RequestPayload is assumed to be URL-encoded by this time
		// In the case of using a streamed file, you must explicitly set the Content-Type, because RequestPayload->IsURLEncoded returns false.
		check(!GetHeader(TEXT("Content-Type")).IsEmpty() || RequestPayload->IsURLEncoded());
	}
	else if (Verb == TEXT("PUT"))
	{
	}
	else if (Verb == TEXT("PATCH"))
	{
		checkNoEntry(); // handle this later
	}
	else if (Verb == TEXT("GET"))
	{
	}
	else if (Verb == TEXT("HEAD"))
	{
	}
	else if (Verb == TEXT("DELETE"))
	{
		check(!GetHeader(TEXT("Content-Type")).IsEmpty() || RequestPayload->IsURLEncoded());
	}
	else
	{
		UE_LOG(LogHttp, Fatal, TEXT("Unsupported verb '%s'"), *Verb);
		UE_DEBUG_BREAK();
	}

	BytesSent.store(0);
	TotalBytesSent.store(0);

	Handle->Init();

	UE_MakeHTTPDataRequest(this);

	// Set connection timeout in seconds
	int32 HttpConnectionTimeout = FHttpModule::Get().GetHttpConnectionTimeout();
	check(HttpConnectionTimeout > 0);
	// todo: set timeout

	UE_LOG(LogHttp, Log, TEXT("%p: Starting %s request to URL='%s'"), this, *Verb, *URL);

	return true;
}

bool FSimplyStreamHttpRequest::ProcessRequest()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamHttpRequest_ProcessRequest);

	// Clear out response. If this is a re-used request, Response could point to a stale response until SetupRequestHttpThread is called
	ResponseCommon = nullptr;
	LastReportedBytesRead = 0;

	if (!PreProcess())
	{
		return false;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_SimplyStreamHttpAddThreadedRequest);
	// Mark as in-flight to prevent overlapped requests using the same object
	SetStatus(EHttpRequestStatus::Processing);

	// Add to global list while being processed so that the ref counted request does not get deleted
	FHttpModule::Get().GetHttpManager().AddThreadedRequest(SharedThis(this));

	StartProcessTime = FPlatformTime::Seconds();

	UE_LOG(LogHttp, VERBOSE, TEXT("%p: request (handle:%p) has been added to threaded queue for processing"), this, Handle);
	return true;
}

void FSimplyStreamHttpRequest::ClearInCaseOfRetry()
{
	FHttpRequestCommon::ClearInCaseOfRetry();

	// Clear out response. If this is a re-used request, Response could point to a stale response until SetupRequestHttpThread is called
	LastReportedBytesRead = 0;
	TotalBytesRead = 0;
	bAnyHttpActivity = false;
}

bool FSimplyStreamHttpRequest::StartThreadedRequest()
{
	// reset timeout
	ElapsedTime = 0.0f;

	UE_LOG(LogHttp, VERBOSE, TEXT("%p: request (handle:%p) has started threaded processing"), this, Handle);

	return true;
}

bool FSimplyStreamHttpRequest::IsThreadedRequestComplete()
{
	if (bSimplyStreamRequestCompleted && ElapsedTime >= FHttpModule::Get().GetHttpDelayTime())
	{
		return true;
	}

	return false;
}

void FSimplyStreamHttpRequest::TickThreadedRequest(float DeltaSeconds)
{
	ElapsedTime += DeltaSeconds;
}

void FSimplyStreamHttpRequest::AbortRequest()
{
	if (bSimplyStreamRequestCompleted)
	{
		return;
	}

	FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();
	if (HttpManager.IsValidRequest(this))
	{
		HttpManager.CancelThreadedRequest(SharedThis(this));
	}
	else
	{
		FinishRequestNotInHttpManager();
	}
}

void FSimplyStreamHttpRequest::Tick(float DeltaSeconds)
{
	if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnGameThread)
	{
		CheckProgressDelegate();
		BroadcastNewlyReceivedHeaders();
	}
}

void FSimplyStreamHttpRequest::CheckProgressDelegate()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamHttpRequest_CheckProgressDelegate);
	const uint64 CurrentBytesRead = TotalBytesRead.load();
	const uint64 CurrentBytesSent = BytesSent.load();

	const bool bProcessing = CompletionStatus == EHttpRequestStatus::Processing;
	const bool bBytesSentChanged = (CurrentBytesSent != LastReportedBytesSent);
	const bool bBytesReceivedChanged = CurrentBytesRead != LastReportedBytesRead;
	const bool bProgressChanged = bBytesSentChanged || bBytesReceivedChanged;
	if (bProcessing && bProgressChanged)
	{
		LastReportedBytesSent = CurrentBytesSent;
		LastReportedBytesRead = CurrentBytesRead;
		OnRequestProgress64().ExecuteIfBound(SharedThis(this), LastReportedBytesSent, LastReportedBytesRead);
	}
}

void FSimplyStreamHttpRequest::BroadcastNewlyReceivedHeaders()
{
	// Process the headers received on the HTTP thread and merge them into the response's list of headers and then broadcast the new headers
	TPair<FString, FString> NewHeader;
	while (NewlyReceivedHeaders.Dequeue(NewHeader))
	{
		OnHeaderReceived().ExecuteIfBound(SharedThis(this), NewHeader.Key, NewHeader.Value);
	}
}

void FSimplyStreamHttpRequest::MarkAsCompleted(int InFetchCompletionResult)
{
	FetchCompletionResult = InFetchCompletionResult;
	bSimplyStreamRequestCompleted = true;

	StopActivityTimeoutTimer();
}

void FSimplyStreamHttpRequest::FinishRequest()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamHttpRequest_FinishRequest);

	PostProcess();

	CheckProgressDelegate();

	TSharedPtr<FSimplyStreamHttpResponse> Response = StaticCastSharedPtr<FSimplyStreamHttpResponse>(ResponseCommon);

	// if completed, get more info
	if (bSimplyStreamRequestCompleted)
	{
		if (Response.IsValid())
		{
			Response->bSucceeded = (FETCH_OK == FetchCompletionResult);

			// get the information
			Response->SetResponseCode(Handle->GetHttpCode());
			Response->ContentLength = Handle->GetContentLength();

			if (Response->GetResponseCode() <= 0 && URL.StartsWith(TEXT("Http"), ESearchCase::IgnoreCase))
			{
				UE_LOG(LogHttp, Warning, TEXT("%p: invalid HTTP response code received. URL: %s, HTTP code: %d, content length: %llu, actual payload size: %llu"),
					this, *GetURL(), Response->GetResponseCode(), Response->ContentLength, TotalBytesRead.load());
				Response->bSucceeded = false;
			}
		}
	}

	// if just finished, mark as stopped async processing
	if (Response.IsValid())
	{
		// Broadcast any headers we haven't broadcast yet
		// If using EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread, we don't add to NewlyReceivedHeaders and will have already broadcast
		if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnGameThread)
		{
			BroadcastNewlyReceivedHeaders();
		}
		Response->bIsReady = true;
	}

	//wonder_log::info("Response.IsValid(): {} Response.bSucceeded: {} bSimplyStreamRequestCompleted: {} GetHttpCode: {} GetContentLength: {}", Response.IsValid(), Response->bSucceeded, bSimplyStreamRequestCompleted.load(), Handle->GetHttpCode(), Handle->GetContentLength());

	const bool bSucceeded = Response.IsValid() && Response->bSucceeded;

	if (bSucceeded)
	{
		// log info about error responses to identify failed downloads
		if (UE_LOG_ACTIVE(LogHttp, VERBOSE))
		{
			UE_LOG(LogHttp, Warning, TEXT("%p: request has been successfully processed. URL: %s, HTTP code: %d, content length: %llu, actual payload size: %llu, elapsed: %.2fs"),
				this, *GetURL(), Response->GetResponseCode(), Response->ContentLength, TotalBytesRead.load(), ElapsedTime);

			TArray<FString> AllHeaders = Response->GetAllHeaders();
			for (TArray<FString>::TConstIterator It(AllHeaders); It; ++It)
			{
				const FString& HeaderStr = *It;
				if (!HeaderStr.StartsWith(TEXT("Authorization")) && !HeaderStr.StartsWith(TEXT("Set-Cookie")))
				{
					UE_LOG(LogHttp, VERBOSE, TEXT("%p Response Header %s"), this, *HeaderStr);
				}
			}
		}
	}
	else
	{
		if (FetchCompletionResult != FETCH_OK)
		{
			UE_LOG(LogHttp, Warning, TEXT("%p: request failed, fetch error: %d"), this, (int32)FetchCompletionResult);
		}

		// Mark last request attempt as completed but failed
		if (bCanceled)
		{
			SetFailureReason(EHttpFailureReason::Cancelled);
		}
		else if (bTimedOut)
		{
			SetFailureReason(EHttpFailureReason::TimedOut);
		}
		else if (bActivityTimedOut)
		{
			SetFailureReason(EHttpFailureReason::ConnectionError);
		}
		else if (bSimplyStreamRequestCompleted)
		{
			SetFailureReason(EHttpFailureReason::Other);
		}
		else
		{
			SetFailureReason(EHttpFailureReason::Other);
		}

	}

	OnFinishRequest(bSucceeded);

	if (!bSucceeded)
	{
		// Delegate needs to know about the errors -- so clear out Response (since connection failed) afterwards...
		ResponseCommon = nullptr;
		TotalBytesRead = 0;
	}
}

void FSimplyStreamHttpRequest::CleanupRequest()
{
	CloseRequestPayloadDefaultImpl();
}

FHttpResponsePtr FSimplyStreamHttpRequest::CreateResponse()
{
	return MakeShared<FSimplyStreamHttpResponse>(*this);
}

// FSimplyStreamHttpRequest

FSimplyStreamHttpResponse::FSimplyStreamHttpResponse(const FSimplyStreamHttpRequest& InRequest)
	: FHttpResponseCommon(InRequest)
	, ContentLength(0)
	, bIsReady(0)
	, bSucceeded(0)
{
}

FString FSimplyStreamHttpResponse::GetHeader(const FString& HeaderName) const
{
	FString Result;
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached header [%s]. Response still processing. %s"), *HeaderName, *GetURL());
	}
	else
	{
		const FString* Header = Headers.Find(HeaderName);
		if (Header != NULL)
		{
			Result = *Header;
		}
	}
	return Result;
}

TArray<FString> FSimplyStreamHttpResponse::GetAllHeaders() const
{
	TArray<FString> Result;
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached headers. Response still processing. %s"), *GetURL());
	}
	else
	{
		Result.Reserve(Headers.Num());
		for (const TPair<FString, FString>& It : Headers)
		{
			Result.Emplace(FSimplyStreamHttpRequest::CombineHeaderKeyValue(It.Key, It.Value));
		}
	}
	return Result;
}

FString FSimplyStreamHttpResponse::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

uint64 FSimplyStreamHttpResponse::GetContentLength() const
{
	return ContentLength;
}

const TArray<uint8>& FSimplyStreamHttpResponse::GetContent() const
{
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %s"), *GetURL());
	}
	return Payload;
}

FString FSimplyStreamHttpResponse::GetContentAsString() const
{
	// Content is NOT null-terminated; we need to specify lengths here
	FUTF8ToTCHAR TCHARData(reinterpret_cast<const ANSICHAR*>(Payload.GetData()), Payload.Num());
	return FString(TCHARData.Length(), TCHARData.Get());
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
