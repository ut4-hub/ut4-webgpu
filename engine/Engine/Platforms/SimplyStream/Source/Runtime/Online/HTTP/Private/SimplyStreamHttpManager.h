// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpManager.h"

class FHttpThread;

class FSimplyStreamHttpManager : public FHttpManager
{
public:
	FSimplyStreamHttpManager();
	static void Init();
	static void Shutdown();
	static bool IsInit();
protected:
	virtual FHttpThreadBase* CreateHttpThread() override;
private:
    static bool bIsInitialized;
};