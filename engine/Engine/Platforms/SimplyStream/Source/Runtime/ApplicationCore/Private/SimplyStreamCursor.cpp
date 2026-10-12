// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamCursor.h"

FSimplyStreamCursor::FSimplyStreamCursor()
{
	// Set the default cursor
	SetType( EMouseCursor::Default );

	CursorStatus = true;
	LockStatus = false;
}


FSimplyStreamCursor::~FSimplyStreamCursor()
{
}

FVector2D FSimplyStreamCursor::GetPosition() const
{
	return Position;
}

void FSimplyStreamCursor::SetPosition( const int32 X, const int32 Y )
{
	Position.Set(X, Y);
#if 0 // wndr-todo: custom-cursor
	EM_ASM({
		var x = $0;
		var y = $1;
		var cursor = document.getElementById("cursor");
		cursor.style.left = x + "px";
		cursor.style.top = y + "px";
	}, X, Y);
#endif
}

void FSimplyStreamCursor::SetType( const EMouseCursor::Type InNewCursor )
{
	CurrentType = InNewCursor;

	// TODO: use .css to correctly change cursor types in javascript.
	if ( InNewCursor == EMouseCursor::None)
		CursorStatus = false;
	else
		CursorStatus  = true;

}

void FSimplyStreamCursor::GetSize( int32& Width, int32& Height ) const
{
	Width = 16;
	Height = 16;
}

void FSimplyStreamCursor::Show( bool bShow )
{
	CursorStatus = bShow;
}

void FSimplyStreamCursor::Lock( const RECT* const Bounds )
{
	if(Bounds == NULL)
	{
		LockStatus = false;
	}
	else
	{
		LockStatus = true;
	}
}
