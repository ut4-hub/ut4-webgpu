// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/Platform.h"

#if defined(PLATFORM_WASM) && PLATFORM_WASM

#include "HAL/UnrealMemory.h"
#include "oodle2net.h"

OOFUNC1 OO_SINTa OOFUNC2 OodleNetwork1_Shared_Size(OO_S32)
{
	return 1;
}

OOFUNC1 OO_SINTa OOFUNC2 OodleNetwork1UDP_State_Size(void)
{
	return 1;
}

OOFUNC1 OO_SINTa OOFUNC2 OodleNetwork1_CompressedBufferSizeNeeded(OO_SINTa RawLen)
{
	return RawLen;
}

OOFUNC1 void OOFUNC2 OodleNetwork1_Shared_SetWindow(OodleNetwork1_Shared*, OO_S32, const void*, OO_S32)
{
}

OOFUNC1 OO_SINTa OOFUNC2 OodleNetwork1UDP_Encode(
	const OodleNetwork1UDP_State*,
	const OodleNetwork1_Shared*,
	const void* Raw,
	OO_SINTa RawLen,
	void* Comp)
{
	if (RawLen > 0)
	{
		FMemory::Memcpy(Comp, Raw, RawLen);
	}
	return RawLen;
}

OOFUNC1 OO_BOOL OOFUNC2 OodleNetwork1UDP_Decode(
	const OodleNetwork1UDP_State*,
	const OodleNetwork1_Shared*,
	const void* Comp,
	OO_SINTa CompLen,
	void* Raw,
	OO_SINTa RawLen)
{
	if (CompLen != RawLen)
	{
		return 0;
	}
	if (RawLen > 0)
	{
		FMemory::Memcpy(Raw, Comp, RawLen);
	}
	return 1;
}

OOFUNC1 OO_BOOL OOFUNC2 OodleNetwork1UDP_State_Uncompact_ForVersion(
	OodleNetwork1UDP_State*,
	const OodleNetwork1UDP_StateCompacted*,
	OO_S32)
{
	return 1;
}

#endif
