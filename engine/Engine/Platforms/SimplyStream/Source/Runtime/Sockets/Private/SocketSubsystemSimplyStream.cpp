// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemSimplyStream.h"
#include "SocketSubsystemModule.h"
#include "Modules/ModuleManager.h"


/* Static initialization
*****************************************************************************/

FSocketSubsystemSimplyStream* FSocketSubsystemSimplyStream::SocketSingleton = NULL;

/* FSocketBSD implementation
*****************************************************************************/

bool FSimplyStreamSocket::SetNonBlocking(bool bIsNonBlocking)
{
	ensureMsgf(bIsNonBlocking, TEXT("Can't have blocking sockets on SimplyStream"));
    return true;
}

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(TEXT("SimplyStream"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FSocketSubsystemSimplyStream* SocketSubsystem = FSocketSubsystemSimplyStream::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemSimplyStream::Destroy();
		return NAME_None;
	}

}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("SimplyStream")));
	FSocketSubsystemSimplyStream::Destroy();
}

/**
 * Singleton interface for the Android socket subsystem
 * @return the only instance of the Android socket subsystem
 */
FSocketSubsystemSimplyStream* FSocketSubsystemSimplyStream::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemSimplyStream();
	}

	return SocketSingleton;
}

/**
 * Destroy the singleton Android socket subsystem
 */
void FSocketSubsystemSimplyStream::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}


bool FSocketSubsystemSimplyStream::Init(FString& Error)
{
	return true;
}

/**
 * Performs SimplyStream specific socket clean up
 */
void FSocketSubsystemSimplyStream::Shutdown(void)
{
}


/**
 * @return Whether the device has a properly configured network device or not
 */
bool FSocketSubsystemSimplyStream::HasNetworkDevice()
{
	return true;
}


/**
 * Create FSimplyStreamSocket as a FSocketBSD object
 */
class FSocketBSD* FSocketSubsystemSimplyStream::InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, const FName& SocketProtocol)
{
	// return a new socket object
	return new FSimplyStreamSocket(Socket, SocketType, SocketDescription, SocketProtocol, this);
}

TSharedPtr<FInternetAddr> FSocketSubsystemSimplyStream::GetAddressFromString(const FString& InAddress)
{
	const bool bLooksLikeIPv6Literal = InAddress.Contains(TEXT(":"));
	bool bLooksLikeIPv4Literal = !InAddress.IsEmpty();
	for (TCHAR Character : InAddress)
	{
		if (!FChar::IsDigit(Character) && Character != TEXT('.'))
		{
			bLooksLikeIPv4Literal = false;
			break;
		}
	}

	if (bLooksLikeIPv4Literal || bLooksLikeIPv6Literal)
	{
		return FSocketSubsystemBSD::GetAddressFromString(InAddress);
	}

	return nullptr;
}

/**
 * Translates an ESocketAddressInfoFlags into a value usable by getaddrinfo
 */
int32 FSocketSubsystemSimplyStream::GetAddressInfoHintFlag(EAddressInfoFlags InFlags) const
{
	// As of writing, emscripten does not support AI_ADDRCONFIG. It is marked as an usable flag,
	// however if it is set, GAI will fail out with a bad name flag.
	// As such, remove the flag from any potential queries.
	EAddressInfoFlags ModifiedInFlags = (InFlags & ~EAddressInfoFlags::OnlyUsableAddresses);
	return FSocketSubsystemBSD::GetAddressInfoHintFlag(ModifiedInFlags);
}
