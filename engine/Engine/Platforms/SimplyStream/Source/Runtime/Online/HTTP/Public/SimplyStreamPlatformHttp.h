// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformHttp.h"

/**
 * Platform specific Http implementations
 */
class FSimplyStreamPlatformHttp : public FGenericPlatformHttp
{
public:

	/**
	 * Platform initialization step
	 */
	static HTTP_API void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return NULL if default implementation is to be used
	 */
	static FHttpManager* CreatePlatformHttpManager();

	/**
	 * Platform shutdown step
	 */
	static void Shutdown();

	/**
	 * Creates a new Http request instance for the current platform
	 *
	 * @return request object
	 */
	static IHttpRequest* ConstructRequest();
	static HTTP_API TOptional<FString> GetOperatingSystemProxyAddress() { return {}; }
	static HTTP_API bool IsOperatingSystemProxyInformationSupported() { return false; }
};


typedef FSimplyStreamPlatformHttp FPlatformHttp;