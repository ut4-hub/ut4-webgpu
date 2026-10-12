// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamInputInterface.h"
#include "SimplyStreamCursor.h"
#include "HAL/OutputDevices.h"
#include "HAL/PlatformTime.h"
//#include "archive.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSimplyStream, Log, All);
DEFINE_LOG_CATEGORY(LogSimplyStream);
DEFINE_LOG_CATEGORY_STATIC(LogSimplyStreamInput, Log, All);

THIRD_PARTY_INCLUDES_START
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END

#ifndef KEEP_IN_MODULE
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#endif

#define AxisMappingCap 4
static FGamepadKeyNames::Type AxisMapping[AxisMappingCap] =
{
	FGamepadKeyNames::LeftAnalogX,
	FGamepadKeyNames::LeftAnalogY,
	FGamepadKeyNames::RightAnalogX,
	FGamepadKeyNames::RightAnalogY
};

// Axis Mapping, reversed or not.
static int Reversed[4] =
{
	1,
	-1,
	1,
	-1
};

// all are digital except Left and Right Trigger Analog.
static FGamepadKeyNames::Type ButtonMapping[SIMPLYSTREAM_INPUT_INTERFACE_BUTTON_MAPPING_CAP] = {};

TSharedRef< FSimplyStreamInputInterface > FSimplyStreamInputInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor )
{
	ButtonMapping[0] = FGamepadKeyNames::FaceButtonBottom;
	ButtonMapping[1] = FGamepadKeyNames::FaceButtonRight;
	ButtonMapping[2] = FGamepadKeyNames::FaceButtonLeft;
	ButtonMapping[3] = FGamepadKeyNames::FaceButtonTop;
	ButtonMapping[4] = FGamepadKeyNames::LeftShoulder;
	ButtonMapping[5] = FGamepadKeyNames::RightShoulder;
	ButtonMapping[6] = FGamepadKeyNames::LeftTriggerThreshold;
	ButtonMapping[7] = FGamepadKeyNames::RightTriggerThreshold;
	ButtonMapping[8] = FGamepadKeyNames::SpecialLeft;
	ButtonMapping[9] = FGamepadKeyNames::SpecialRight;
	ButtonMapping[10] = FGamepadKeyNames::LeftThumb;
	ButtonMapping[11] = FGamepadKeyNames::RightThumb;
	ButtonMapping[12] = FGamepadKeyNames::DPadUp;
	ButtonMapping[13] = FGamepadKeyNames::DPadDown;
	ButtonMapping[14] = FGamepadKeyNames::DPadLeft;
	ButtonMapping[15] = FGamepadKeyNames::DPadRight;

	return MakeShareable( new FSimplyStreamInputInterface( InMessageHandler, InCursor ) );
}
InputState inputState[4];
KEEP_IN_MODULE void setInputState(int id, const char* type, int type_length, double axis0, double axis1, double axis2, double axis3, bool button0, bool button1, bool button2, bool button3, bool button4, bool button5, bool button6, bool button7, bool button8, bool button9, bool button10, bool button11, bool button12, bool button13, bool button14, bool button15, bool button16) {
	inputState[id] = { true, id, axis0,axis1,axis2,axis3,button0,button1,button2,button3,button4,button5,button6,button7,button8,button9,button10,button11,button12,button13,button14,button15,button16, {type, (uint32_t)type_length} };
}
EM_BOOL gamepad_callback_browser_thread(int eventType, const EmscriptenGamepadEvent* e, void* userData) {
	if (e->connected) {
		inputState[e->index].active = true;
		//UE_LOG(LogSimplyStreamApplication, Display, TEXT("New Gamepad Detected at index %i!"), e->index);
		return 1;
	}
	else {
		inputState[e->index].active = false;
		//UE_LOG(LogSimplyStreamApplication, Display, TEXT("Gamepad Connection Lost from index %i!"), e->index);
		return 0;
	}
}
FSimplyStreamInputInterface::FSimplyStreamInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor )
	: MessageHandler( InMessageHandler )
	, Cursor( InCursor )
{
	extern bool runningNode;
	if (runningNode) {
		//EM_ASM(console.log('Not enabling local input as this is a RCS server'));
		return;
	}

	// wndr-todo: add this back in when we work on hybrid support for 5. We need to make sure it doesnt block the main thread.
	//emscripten_set_mousemove_callback("#canvas", (void*)&Cursor, 1, mouse_move_callback);

	//UE_LOG(LogSimplyStreamApplication, Display, TEXT("Gamepad Connected Callback Set Succesfully: \"%s\"\n"), emscripten_set_gamepadconnected_callback_on_thread(this, 0, gamepad_callback_browser_thread, //EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD) == EMSCRIPTEN_RESULT_SUCCESS ? TEXT("TRUE") : TEXT("FALSE"));
    //UE_LOG(LogSimplyStreamApplication, Display, TEXT("Gamepad Disconnected Callback Set Succesfully: \"%s\"\n"), emscripten_set_gamepadconnected_callback_on_thread(this, 0, gamepad_callback_browser_thread, //EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD) == EMSCRIPTEN_RESULT_SUCCESS ? TEXT("TRUE") : TEXT("FALSE"));
#if 0 // wndr-todo: re-enable at a later time
	MAIN_THREAD_EM_ASM({
		//console.log('before loop');
		setInterval(()=>{
		var gamepads = navigator.getGamepads();
		//console.log('looping', gamepads);
		[...gamepads].forEach((gp) => {
			if (gp) {
			_setInputState(gp.index, gp.id, gp.id.length, gp.axes[0],gp.axes[1],gp.axes[2],gp.axes[3], gp.buttons[0].pressed, gp.buttons[1].pressed, gp.buttons[2].pressed, gp.buttons[3].pressed, gp.buttons[4].pressed, gp.buttons[5].pressed, gp.buttons[6].pressed, gp.buttons[7].pressed, gp.buttons[8].pressed, gp.buttons[9].pressed, gp.buttons[10].pressed, gp.buttons[11].pressed, gp.buttons[12].pressed, gp.buttons[13].pressed, gp.buttons[14].pressed, gp.buttons[15].pressed, gp.buttons[16].pressed);
			//console.log(gp.index, gp.id, gp.id.length, gp.axes[0], gp.axes[1], gp.axes[2], gp.axes[3], gp.buttons[0].pressed, gp.buttons[1].pressed, gp.buttons[2].pressed, gp.buttons[3].pressed, gp.buttons[4].pressed, gp.buttons[5].pressed, gp.buttons[6].pressed, gp.buttons[7].pressed, gp.buttons[8].pressed, gp.buttons[9].pressed, gp.buttons[10].pressed, gp.buttons[11].pressed, gp.buttons[12].pressed, gp.buttons[13].pressed, gp.buttons[14].pressed, gp.buttons[15].pressed, gp.buttons[16].pressed);
			}
		})
		}, 10);
	});
#endif
}

void FSimplyStreamInputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}

void FSimplyStreamInputInterface::SendControllerEvents()
{
	const double CurrentTime = FPlatformTime::Seconds();

	int _CurrentGamePad = 0;
	FPlatformUserId PlatformID = PLATFORMUSERID_NONE;
	for (auto& state : inputState) {
		auto CurrentGamePad = _CurrentGamePad++;
		FInputDeviceId DeviceID;
		DeviceID.CreateFromInternalId(CurrentGamePad);
		if (!state.active) continue;
		//EM_ASM(console.log('axis[0] =', $0, 'axis[1] =', $1, 'axis[2] =', $2, 'axis[3] =', $3), state.axis[0], state.axis[1], state.axis[2], state.axis[3]);
		MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, PlatformID, DeviceID, state.axis[0]);
		MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, PlatformID, DeviceID, state.axis[1] * -1);
		MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogX, PlatformID, DeviceID, state.axis[2]);
		MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogY, PlatformID, DeviceID, state.axis[3] * -1);
		const float RepeatDelta = 0.2f;
		for (int CurrentButton = 0; CurrentButton < SIMPLYSTREAM_INPUT_INTERFACE_BUTTON_MAPPING_CAP; ++CurrentButton) {
			// trigger for digital buttons.
				if (state.pressed[CurrentButton])
				{
					if (state.pressed[CurrentButton] != PrevGamePadState[CurrentGamePad].pressed[CurrentButton])
					{
						//EM_ASM({ console.log('button pressed', $0) }, CurrentButton);
						MessageHandler->OnControllerButtonPressed(ButtonMapping[CurrentButton], PlatformID, DeviceID, false);
						LastPressedTime[CurrentGamePad][CurrentButton] = CurrentTime;
					}
					else if (CurrentTime - LastPressedTime[CurrentGamePad][CurrentButton] > RepeatDelta)
					{
						//EM_ASM({ console.log('button held', $0) }, CurrentButton);
						MessageHandler->OnControllerButtonPressed(ButtonMapping[CurrentButton], PlatformID, DeviceID, true);
						LastPressedTime[CurrentGamePad][CurrentButton] = CurrentTime;
					}
				}
				else if (state.pressed[CurrentButton] != PrevGamePadState[CurrentGamePad].pressed[CurrentButton])
				{
					//EM_ASM({ console.log('button unpressed', $0) }, CurrentButton);
					MessageHandler->OnControllerButtonReleased(ButtonMapping[CurrentButton], PlatformID, DeviceID, false);
				}
		}
		PrevGamePadState[CurrentGamePad] = state;
	}
}
