// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamEventLoopHttpThread.h"
#include "Stats/Stats.h"
#include "Http.h"
#include "SimplyStreamHTTP.h"
#include "SimplyStreamHttpManager.h"

#include "EventLoop/EventLoop.h"

FSimplyStreamIOManagerIOAccess::FSimplyStreamIOManagerIOAccess(FSimplyStreamIOManager& InIOManager)
	: IOManager(InIOManager)
{
}

FSimplyStreamIOManager::FSimplyStreamIOManager(UE::EventLoop::IEventLoop& InEventLoop, FParams&& InParams)
	: IOAccess(*this)
	, EventLoop(InEventLoop)
	, Params(MoveTemp(InParams))
{
}

bool FSimplyStreamIOManager::Init()
{
	check(Params.ProcessFetchRequests);
	return true;
}

void FSimplyStreamIOManager::Shutdown()
{
}

void FSimplyStreamIOManager::Notify()
{
}

void FSimplyStreamIOManager::Poll(FTimespan WaitTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSimplyStreamIOManager_Poll);

	int RunningRequests = -1;

	Params.ProcessFetchRequests();
}

FSimplyStreamEventLoopHttpThread::FSimplyStreamEventLoopHttpThread()
{
}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
bool FSimplyStreamEventLoopHttpThread::StartThreadedRequest(FHttpRequestCommon* Request)
{
	FSimplyStreamHttpRequest* SimplyStreamRequest = static_cast<FSimplyStreamHttpRequest*>(Request);
	SimplyStreamRequest->SetupRequestHttpThread();

	return FEventLoopHttpThread::StartThreadedRequest(Request);
}

void FSimplyStreamEventLoopHttpThread::CompleteThreadedRequest(FHttpRequestCommon* Request)
{
}

void FSimplyStreamEventLoopHttpThread::CreateEventLoop()
{
	UE::EventLoop::TEventLoop<FSimplyStreamIOManager>::FParams EventLoopParams;
	EventLoopParams.IOManagerParams.ProcessFetchRequests = [this](){ ProcessFetchRequests(); };
	EventLoop.Emplace(MoveTemp(EventLoopParams));
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
void FSimplyStreamEventLoopHttpThread::DestroyEventLoop()
{
	EventLoop.Reset();
}

void FSimplyStreamEventLoopHttpThread::UpdateEventLoopConfigs()
{
}

UE::EventLoop::IEventLoop* FSimplyStreamEventLoopHttpThread::GetEventLoop()
{
	return EventLoop.IsSet() ? &*EventLoop : nullptr;
}

UE::EventLoop::IEventLoop& FSimplyStreamEventLoopHttpThread::GetEventLoopChecked()
{
	return *EventLoop;
}

void FSimplyStreamEventLoopHttpThread::ProcessFetchRequests()
{
}