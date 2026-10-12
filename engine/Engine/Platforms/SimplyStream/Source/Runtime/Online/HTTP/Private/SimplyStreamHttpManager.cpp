// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamHttpManager.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"
#include "Misc/Paths.h"
#include "Misc/Fork.h"

#include "SimplyStreamHttpThread.h"
#if 0
#include "SimplyStreamMultiPollEventLoopHttpThread.h"
#include "SimplyStreamMultiWaitEventLoopHttpThread.h"
#include "SimplyStreamSocketEventLoopHttpThread.h"
#endif
#include "SimplyStreamHTTP.h"
#include "Misc/OutputDeviceRedirector.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"

#include "Http.h"
#include "wonder_log.h"

bool FSimplyStreamHttpManager::bIsInitialized = false;

FSimplyStreamHttpManager::FSimplyStreamHttpManager() {
}

bool FSimplyStreamHttpManager::IsInit()
{
    return bIsInitialized;
}
void FSimplyStreamHttpManager::Init()
{
	if (bIsInitialized)
    {
        // Already initialized
        return;
    }
    // Initialize any necessary components for Emscripten (if any)
    bIsInitialized = true;
}

void FSimplyStreamHttpManager::Shutdown()
{
    if (!bIsInitialized)
        return;

    // Clean up resources if necessary
    bIsInitialized = false;
}

FHttpThreadBase* FSimplyStreamHttpManager::CreateHttpThread()
{
	// Do any of these make sense for emscripten?
#if 0
	if (bUseEventLoop)
	{
#if WITH_CURL_MULTIPOLL
		UE_LOG(LogInit, Log, TEXT("CreateHttpThread using FSimplyStreamMultiPollEventLoopHttpThread"));
		return new FSimplyStreamMultiPollEventLoopHttpThread();

#elif WITH_CURL_MULTISOCKET
		UE_LOG(LogInit, Log, TEXT("CreateHttpThread using FSimplyStreamSocketEventLoopHttpThread"));
		return new FSimplyStreamSocketEventLoopHttpThread();

#elif WITH_CURL_MULTIWAIT
		UE_LOG(LogInit, Log, TEXT("CreateHttpThread using FSimplyStreamMultiWaitEventLoopHttpThread"));
		return new FSimplyStreamMultiWaitEventLoopHttpThread();
#endif
	}
#endif
	return new FSimplyStreamHttpThread();
}