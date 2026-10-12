// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	SimplyStreamTLS.h: SimplyStream platform TLS (Thread local storage and thread ID) functions
==============================================================================================*/

#pragma once
#include "Containers/Array.h"
#include "GenericPlatform/GenericPlatformTLS.h"

/**
 * SimplyStream implementation of the TLS OS functions
 */
struct CORE_API FSimplyStreamTLS : public FGenericPlatformTLS
{
#ifndef __EMSCRIPTEN_PTHREADS__
	/**
	 * In singlethreaded Emscripten builds, returns a
	 * regular array to serve as an emulated TLS storage
	 * (Emscripten does support the pthread API even in
	 * singlethreaded builds, so the same set/getspecific
	 * code below could be used there, but it's faster
	 * to use this this approach and reduces code size
	 * a tiny bit.
	 */
	static TArray<void*>& GetFakeTLSArray()
	{
		static TArray<void*> TLS;
		return TLS;
	}
#endif

	/**
	 * Returns the currently executing thread's id
	 */
	static uint32 GetCurrentThreadId(void);

	/**
	 * Allocates a thread local store slot
	 */
	static uint32 AllocTlsSlot(void);

	/**
	 * Sets a value in the specified TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 * @param Value the value to store in the slot
	 */
	static void SetTlsValue(uint32 SlotIndex, void* Value);

	/**
	 * Reads the value stored at the specified TLS slot
	 *
	 * @return the value stored in the slot
	 */
	static void* GetTlsValue(uint32 SlotIndex);

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static void FreeTlsSlot(uint32 SlotIndex);

protected:

};

typedef FSimplyStreamTLS FPlatformTLS;
