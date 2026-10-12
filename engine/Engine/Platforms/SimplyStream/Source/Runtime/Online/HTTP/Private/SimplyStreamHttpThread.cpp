// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamHttpThread.h"
#include "Stats/Stats.h"
#include "Http.h"
#include "SimplyStreamHTTP.h"
#include "SimplyStreamHttpManager.h"
#include "wonder_log.h"

FSimplyStreamHttpThread::FSimplyStreamHttpThread()
{
	//wonder_log::info("FSimplyStreamHttpThread");
	//StartThread();
}

void FSimplyStreamHttpThread::HttpThreadTick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamHttpThread_HttpThreadTick);
	check(FSimplyStreamHttpManager::IsInit());

	FLegacyHttpThread::HttpThreadTick(DeltaSeconds);
}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool FSimplyStreamHttpThread::StartThreadedRequest(FHttpRequestCommon* Request)
{
	FSimplyStreamHttpRequest* SimplyStreamRequest = static_cast<FSimplyStreamHttpRequest*>(Request);
	SimplyStreamRequest->SetupRequestHttpThread();

	return FLegacyHttpThread::StartThreadedRequest(Request);
}

void FSimplyStreamHttpThread::CompleteThreadedRequest(FHttpRequestCommon* Request)
{
	FSimplyStreamHttpRequest* SimplyStreamRequest = static_cast<FSimplyStreamHttpRequest*>(Request);
	FetchData* Handle = SimplyStreamRequest->GetFetchDataHandle();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS