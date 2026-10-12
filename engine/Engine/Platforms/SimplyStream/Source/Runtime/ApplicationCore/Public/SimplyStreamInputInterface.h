// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/ICursor.h"
#include "Templates/SharedPointer.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "Containers/BitArray.h"
THIRD_PARTY_INCLUDES_START
#include <emscripten/html5.h>
#include <string>
THIRD_PARTY_INCLUDES_END

#define SIMPLYSTREAM_INPUT_INTERFACE_MAX_CONTROLLERS 4
#define SIMPLYSTREAM_INPUT_INTERFACE_BUTTON_MAPPING_CAP 16

struct InputState {
	bool active = false;
	int id = 0;
	double axis[4];
	bool pressed[17];
	std::string type;
};

/**
 * Interface class for SimplyStream input devices
 */
class FSimplyStreamInputInterface
{
public:
	/** Initialize the interface */
	static TSharedRef< FSimplyStreamInputInterface > Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor );

public:

	~FSimplyStreamInputInterface() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

	/**
	 * Poll for controller state and send events if needed
	 */
	void SendControllerEvents();


private:

	FSimplyStreamInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor );


private:

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
	const TSharedPtr< ICursor > Cursor;

	TBitArray<FDefaultBitArrayAllocator> KeyStates;

	InputState PrevGamePadState[SIMPLYSTREAM_INPUT_INTERFACE_MAX_CONTROLLERS];
	double LastPressedTime[SIMPLYSTREAM_INPUT_INTERFACE_MAX_CONTROLLERS][SIMPLYSTREAM_INPUT_INTERFACE_BUTTON_MAPPING_CAP];

};
