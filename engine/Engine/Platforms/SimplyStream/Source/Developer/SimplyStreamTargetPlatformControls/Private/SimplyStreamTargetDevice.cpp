// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamTargetDevice.h"
#include "Interfaces/ITargetPlatform.h"



/* ITargetDevice interface
 *****************************************************************************/

bool FSimplyStreamTargetDevice::Connect( )
{
	return false;
}




void FSimplyStreamTargetDevice::Disconnect( )
{
}

FTargetDeviceId FSimplyStreamTargetDevice::GetId( ) const
{
	return FTargetDeviceId(TargetPlatform.PlatformName(), Path);
}


FString FSimplyStreamTargetDevice::GetName( ) const
{
	return Name;
}


FString FSimplyStreamTargetDevice::GetOperatingSystemName( )
{
	return TEXT("SimplyStream Browser");
}


int32 FSimplyStreamTargetDevice::GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos )
{
	return OutProcessInfos.Num();
}


bool FSimplyStreamTargetDevice::PowerOff( bool Force )
{
	return false;
}


bool FSimplyStreamTargetDevice::PowerOn( )
{
	return false;
}



bool FSimplyStreamTargetDevice::Reboot( bool bReconnect )
{
	return false;
}

bool FSimplyStreamTargetDevice::SupportsFeature( ETargetDeviceFeatures Feature ) const
{
	switch (Feature)
	{
	case ETargetDeviceFeatures::MultiLaunch:
		return true;

	default:
		return false;
	}
}


void FSimplyStreamTargetDevice::SetUserCredentials( const FString& UserName, const FString& UserPassword )
{
}

bool FSimplyStreamTargetDevice::GetUserCredentials( FString& OutUserName, FString& OutUserPassword )
{
	return false;
}

bool FSimplyStreamTargetDevice::TerminateProcess( const int64 ProcessId )
{
	return false;
}
