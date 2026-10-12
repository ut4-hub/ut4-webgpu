// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamWindow.h"
#include "HAL/OutputDevices.h"

THIRD_PARTY_INCLUDES_START
	#include <emscripten/emscripten.h>
	#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogSimplyStreamWindow, Log, All);

FSimplyStreamWindow::~FSimplyStreamWindow()
{
	//    Use NativeWindow_Destroy() instead.
}

TSharedRef<FSimplyStreamWindow> FSimplyStreamWindow::Make()
{
	return MakeShareable( new FSimplyStreamWindow() );
}

FSimplyStreamWindow::FSimplyStreamWindow()
{
}

bool FSimplyStreamWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();
	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;
	return true;
}


void FSimplyStreamWindow::SetOSWindowHandle(void* InWindow)
{
}


FPlatformRect FSimplyStreamWindow::GetScreenRect()
{
	FPlatformRect ScreenRect;
	ScreenRect.Left = 0;
	ScreenRect.Top = 0;
#if 0
	ScreenRect.Right = 3440;
	ScreenRect.Bottom = 1440;

	return ScreenRect;
#endif
	int Width, Height;
	emscripten_get_canvas_element_size("#canvas", &Width, &Height);
	if (Width < 16 || Height < 16)
	{
		// The OffscreenCanvas transferred to this worker thread reports a ~0 size via
		// emscripten_get_canvas_element_size, collapsing the game window and the WebGPU
		// surface texture to 8x8 (the whole UI renders in a corner). Force the declared
		// canvas size and actually resize the OffscreenCanvas so the surface is full-size.
		Width = 1280; Height = 720;
		emscripten_set_canvas_element_size("#canvas", Width, Height);
	}
	UE_LOG(LogSimplyStreamWindow, Warning, TEXT("SimplyStream canvas size Width:%d, Height:%d"), Width, Height);

	CalculateSurfaceSize(NULL,Width,Height);
	ScreenRect.Right = Width;
	ScreenRect.Bottom = Height;
	return ScreenRect;
}

void FSimplyStreamWindow::CalculateSurfaceSize(void* InWindow, int32_t& SurfaceWidth, int32_t& SurfaceHeight)
{
	// ensure the size is divisible by a specified amount
	const int DividableBy = 8;
	SurfaceWidth  = ((SurfaceWidth  + DividableBy - 1) / DividableBy) * DividableBy;
	SurfaceHeight = ((SurfaceHeight + DividableBy - 1) / DividableBy) * DividableBy;
}

EWindowMode::Type FSimplyStreamWindow::GetWindowMode() const
{
	extern bool runningNode;
	if (runningNode) {
		return EWindowMode::Windowed;
	}
//#ifdef __EMSCRIPTEN_PTHREADS__
//	/// XXX TODO Restore this, emscripten_get_fullscreen_status() currently has a moderately high performance impact, so disabled for local testing
//	return EWindowMode::Windowed;
//#else
	//EmscriptenFullscreenChangeEvent fullscreenStatus;
	//EMSCRIPTEN_RESULT r = emscripten_get_fullscreen_status(&fullscreenStatus);
	//return (r == EMSCRIPTEN_RESULT_SUCCESS && fullscreenStatus.isFullscreen) ? EWindowMode::Fullscreen : EWindowMode::Windowed;
	//return EWindowMode::Fullscreen;
	return EWindowMode::Windowed;
//#endif
}

void FSimplyStreamWindow::ReshapeWindow(int32 X, int32 Y, int32 Width, int32 Height)
{
}
