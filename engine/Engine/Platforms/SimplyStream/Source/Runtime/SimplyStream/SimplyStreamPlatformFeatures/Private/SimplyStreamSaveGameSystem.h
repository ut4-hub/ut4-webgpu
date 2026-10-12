// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#if 1
#include "SaveGameSystem.h"

class FSimplyStreamSaveGameSystem : public ISaveGameSystem
{
public:
	FSimplyStreamSaveGameSystem();
	virtual ~FSimplyStreamSaveGameSystem();

	virtual bool DoesSaveSystemSupportMultipleUsers() override
	{
		return false;
	}

	// ISaveGameSystem interface
	virtual bool PlatformHasNativeUI() override
	{
		return false;
	}

	virtual bool DoesSaveGameExist(const TCHAR* Name, const int32 UserIndex) override
	{
		return ESaveExistsResult::OK == DoesSaveGameExistWithResult(Name, UserIndex);
	}

	virtual ESaveExistsResult DoesSaveGameExistWithResult(const TCHAR* Name, const int32 UserIndex) override;

	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data) override;

	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data) override;

	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex) override;

private:
	/**
	 * Initializes the SaveData library then loads and initializes the SaveDialog library
	 */
	void Initialize();

	/**
	 * Terminates and unloads the SaveDialog library then terminates the SaveData library
	 */
	void Shutdown();

	/**
	 * Get the path to save game file for the given name
	 */
	const char* GetSaveGamePath(const TCHAR* Name, const int32 UserIndex);

	/**
	 * Get the names of all save games for the given user
	 */
	bool GetSaveGameNames(TArray<FString>& FoundSaves, const int32 UserIndex);
};
#endif