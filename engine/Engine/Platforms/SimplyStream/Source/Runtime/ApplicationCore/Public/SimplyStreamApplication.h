// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericApplication.h"
#include "SimplyStreamWindow.h"
#include "Misc/CoreMisc.h"

THIRD_PARTY_INCLUDES_START
#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END

/**
 * SimplyStream-specific application implementation.
 */
class FSimplyStreamApplication : public GenericApplication, public FSelfRegisteringExec
{
public:

	static FSimplyStreamApplication* CreateSimplyStreamApplication();

	virtual ~FSimplyStreamApplication() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;

	virtual void SetCapture(const TSharedPtr< FGenericWindow >& InWindow) override;

	TSharedRef< FGenericWindow > MakeWindow();

	// FSelfRegisteringExec
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	EM_BOOL OnKeyEvent(int eventType, const EmscriptenKeyboardEvent *keyEvent);
	EM_BOOL OnMouseEvent(int eventType, const EmscriptenMouseEvent *mouseEvent);
	EM_BOOL OnWheelEvent(int eventType, const EmscriptenWheelEvent *wheelEvent);
	EM_BOOL OnFocusEvent(int eventType, const EmscriptenFocusEvent *focusEvent);
	EM_BOOL OnPointerLockChangeEvent(int eventType, const EmscriptenPointerlockChangeEvent *focusEvent);

	void SimplyStreamHandleKeyChar(const int32_t ActualKey, const bool bIsRepeat);
	void SimplyStreamHandleKeyDown(const int32_t ActualKey, const uint32_t CharCode, const bool bIsRepeat);
	void SimplyStreamHandleKeyUp(const int32_t ActualKey, const uint32_t CharCode);
	void SimplyStreamHandleMousePos(const int32_t PosX, const int32_t PosY, const int32_t DeltaX, const int32_t DeltaY);
	void SimplyStreamHandleLMouseDown(const int32_t x, const int32_t y);
	void SimplyStreamHandleRMouseDown(const int32_t x, const int32_t y);
	void SimplyStreamHandleLMouseUp(const int32_t x, const int32_t y);
	void SimplyStreamHandleRMouseUp(const int32_t x, const int32_t y);
	void SimplyStreamHandleActivate();

	TSharedRef< class FGenericWindow > ApplicationWindow;
private:

	FSimplyStreamApplication();

	TSharedPtr< class FSimplyStreamInputInterface > InputInterface;

	int32 WarmUpTicks;

	int32 WindowWidth;
	int32 WindowHeight;
};
