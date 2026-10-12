// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamPlatformHttp.h"
#include "SimplyStreamHttpManager.h"
#include "SimplyStreamHTTP.h"
#include "wonder_log.h"

void FSimplyStreamPlatformHttp::Init()
{
	FGenericPlatformHttp::Init();

	FSimplyStreamHttpManager::Init();
}

void FSimplyStreamPlatformHttp::Shutdown()
{
}


IHttpRequest* FSimplyStreamPlatformHttp::ConstructRequest()
{
	return new FSimplyStreamHttpRequest();
}

FHttpManager* FSimplyStreamPlatformHttp::CreatePlatformHttpManager()
{
	return new FSimplyStreamHttpManager();
}