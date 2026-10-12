// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BSDSockets/SocketSubsystemBSD.h"
#include "BSDSockets/SocketsBSD.h"
#include "SocketSubsystemPackage.h"

/**
 * SimplyStream sockets, sub-classed from BSD sockets, needed to override a couple of functions
 */
class FSimplyStreamSocket
	: public FSocketBSD
{
public:
	FSimplyStreamSocket(SOCKET InSocket, ESocketType InSocketType, const FString& InSocketDescription, const FName& InProtocolType, ISocketSubsystem * InSubsystem)
		: FSocketBSD(InSocket, InSocketType, InSocketDescription, InProtocolType, InSubsystem)
	{ }

	virtual bool SetNonBlocking(bool bIsNonBlocking) override;
};

/**
 * SimplyStream specific socket subsystem implementation
 */
class FSocketSubsystemSimplyStream : public FSocketSubsystemBSD
{
protected:

	/** Single instantiation of this subsystem */
	static FSocketSubsystemSimplyStream* SocketSingleton;

	/** Whether Init() has been called before or not */
	bool bTriedToInit;

public:

	/**
	 * Singleton interface for this subsystem
	 * @return the only instance of this subsystem
	 */
	static FSocketSubsystemSimplyStream* Create();

	/**
	 * Performs Android specific socket clean up
	 */
	static void Destroy();

public:

	FSocketSubsystemSimplyStream()
		: bTriedToInit(false)
	{
	}

	virtual ~FSocketSubsystemSimplyStream()
	{
	}

	/**
	 * Does Android platform initialization of the sockets library
	 *
	 * @param Error a string that is filled with error information
	 *
	 * @return TRUE if initialized ok, FALSE otherwise
	 */
	virtual bool Init(FString& Error) override;

	/**
	 * Performs platform specific socket clean up
	 */
	virtual void Shutdown() override;

	/**
	 * @return Whether the device has a properly configured network device or not
	 */
	virtual bool HasNetworkDevice() override;

	/**
	 * Create FSimplyStreamSocket as a FSocketBSD object
	 */
	virtual class FSocketBSD* InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, const FName& SocketProtocol) override;

	/**
	 * Parse numeric IP literals only. DNS hostnames are expected to flow through GetAddressInfo
	 * or the WebTransport/WebSocket URL path, not BSD IP literal serialization.
	 */
	virtual TSharedPtr<FInternetAddr> GetAddressFromString(const FString& InAddress) override;

	/**
	 * Translates an ESocketAddressInfoFlags into a value usable by getaddrinfo
	 */
	virtual int32 GetAddressInfoHintFlag(EAddressInfoFlags InFlags) const override;
};
