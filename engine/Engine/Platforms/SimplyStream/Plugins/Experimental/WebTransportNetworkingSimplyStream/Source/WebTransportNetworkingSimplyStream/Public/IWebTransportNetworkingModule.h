// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/UniquePtr.h"
#include "Templates/SharedPointer.h"


class IWebTransportServer;
class INetworkingWebTransport;

/* 
 * Interface for the WebTransportNetworking module. 
 */
class IWebTransportNetworkingModule
	: public IModuleInterface
{
public:
	virtual ~IWebTransportNetworkingModule() = default;

	/**
	 * Create a WebTransport server.
	 *
	 * @return A new WebTransport server, or nullptr if the server couldn't be created.
	 */
	virtual TUniquePtr<IWebTransportServer> CreateServer() = 0;

	/**
	 * Create a WebTransport client connection.
	 *
	 * @return A new WebTransport client connection, or nullptr if the connection couldn't be created.
	 */
	virtual TSharedPtr<INetworkingWebTransport> CreateConnection(const class FInternetAddr& ServerAddress) = 0;
};
