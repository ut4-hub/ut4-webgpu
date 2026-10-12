// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/TargetDeviceId.h"
#include "Interfaces/ITargetDevice.h"

class FSimplyStreamTargetDevice;
class ITargetPlatform;


/**
 * Type definition for shared pointers to instances of FSimplyStreamTargetDevice.
 */
typedef TSharedPtr<class FSimplyStreamTargetDevice, ESPMode::ThreadSafe> FSimplyStreamTargetDevicePtr;

/**
 * Type definition for shared references to instances of FSimplyStreamTargetDevice.
 */
typedef TSharedRef<class FSimplyStreamTargetDevice, ESPMode::ThreadSafe> FSimplyStreamTargetDeviceRef;

/**
 * Implements a SimplyStream target device.
 */
class FSimplyStreamTargetDevice
	: public ITargetDevice
{
public:

	/**
	 * Creates and initializes a new SimplyStream target device.
	 *
	 * @param InTarget - The TMAPI target interface.
	 * @param InTargetManager - The TMAPI target manager.
	 * @param InTargetPlatform - The target platform.
	 * @param InName - The device name.
	 */
	FSimplyStreamTargetDevice( const ITargetPlatform& InTargetPlatform, const FString& InName, const FString& InPath )
		: TargetPlatform(InTargetPlatform),
		  Name(InName),
		  Path(InPath)
	{ }

	/**
	 * Destructor.
	 */
	~FSimplyStreamTargetDevice( )
	{
		Disconnect();
	}


public:

	//~ Begin ITargetDevice Interface

	virtual bool Connect( ) override;

	virtual void Disconnect( ) override;

	virtual ETargetDeviceTypes GetDeviceType( ) const override
	{
		return ETargetDeviceTypes::Browser;
	}

	virtual FTargetDeviceId GetId( ) const override;

	virtual FString GetName( ) const override;

	virtual FString GetOperatingSystemName( ) override;

	virtual int32 GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) override;

	virtual const class ITargetPlatform& GetTargetPlatform( ) const override
	{
		return TargetPlatform;
	}

	virtual bool IsConnected( ) override
	{
		return true;
	}
	virtual bool IsDefault( ) const override
	{
		return true;
	}
	virtual bool PowerOff( bool Force ) override;
	virtual bool PowerOn( ) override;
	virtual bool Reboot( bool bReconnect = false ) override;
	virtual bool SupportsFeature( ETargetDeviceFeatures Feature ) const override;
	virtual bool TerminateProcess( const int64 ProcessId ) override;
	virtual void SetUserCredentials( const FString& UserName, const FString& UserPassword ) override;
	virtual bool GetUserCredentials( FString& OutUserName, FString& OutUserPassword ) override;
	//~ End ITargetDevice Interface

private:

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;

	FString Name;

	FString Path;
};
