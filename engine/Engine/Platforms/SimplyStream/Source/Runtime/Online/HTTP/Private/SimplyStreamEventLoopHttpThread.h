// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "EventLoopHttpThread.h"
#include "EventLoop/EventLoop.h"
#include "Templates/Function.h"

class FHttpRequestCommon;
class FSimplyStreamIOManager;
struct FetchData;

class FSimplyStreamIOManagerIOAccess final : public FNoncopyable
{
public:
	FSimplyStreamIOManagerIOAccess(FSimplyStreamIOManager& InIOManager);

	// Nothing needed for now.

private:
	FSimplyStreamIOManager& IOManager;
};

class FSimplyStreamIOManager final : public UE::EventLoop::IIOManager
{
public:
	using FIOAccess = FSimplyStreamIOManagerIOAccess;

	struct FParams
	{
		FetchData* MultiHandle = nullptr;
		TUniqueFunction<void()> ProcessFetchRequests;
	};

	FSimplyStreamIOManager(UE::EventLoop::IEventLoop& EventLoop, FParams&& Params);
	virtual ~FSimplyStreamIOManager() = default;
	virtual bool Init() override;
	virtual void Shutdown() override;
	virtual void Notify() override;
	virtual void Poll(FTimespan WaitTime) override;

	FIOAccess& GetIOAccess()
	{
		return IOAccess;
	}

private:

	FIOAccess IOAccess;
	UE::EventLoop::IEventLoop& EventLoop;
	FParams Params;
};

class FSimplyStreamEventLoopHttpThread
	: public FEventLoopHttpThread
{
public:

	FSimplyStreamEventLoopHttpThread();

protected:
	//~ Begin FHttpThread Interface
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual bool StartThreadedRequest(FHttpRequestCommon* Request) override;
	virtual void CompleteThreadedRequest(FHttpRequestCommon* Request) override;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	//~ End FHttpThread Interface
protected:
	virtual void CreateEventLoop() override;
	virtual void DestroyEventLoop() override;
	virtual void UpdateEventLoopConfigs() override;
	virtual UE::EventLoop::IEventLoop* GetEventLoop() override;
	virtual UE::EventLoop::IEventLoop& GetEventLoopChecked() override;

	void ProcessFetchRequests();

	TOptional<UE::EventLoop::TEventLoop<FSimplyStreamIOManager>> EventLoop;

	/** Mapping of fetch handles to HTTP requests */
	TMap<FetchData*, FHttpRequestCommon*> HandlesToRequests;
};