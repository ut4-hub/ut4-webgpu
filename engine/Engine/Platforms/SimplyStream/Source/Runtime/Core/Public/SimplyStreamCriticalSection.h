// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformCriticalSection.h"

#ifdef __EMSCRIPTEN_PTHREADS__

#include "HAL/PThreadCriticalSection.h"
#include "HAL/PThreadRWLock.h"

typedef FPThreadsCriticalSection FCriticalSection;
typedef FSystemWideCriticalSectionNotImplemented FSystemWideCriticalSection;
// typedef FPThreadsRWLock FRWLock; // TODO: Replace the line below with this - using the generic platform rw lock for now to keep it simple.
typedef TGenericPlatformRWLock<FPThreadsCriticalSection> FRWLock;

#else

class FSimplyStreamCriticalSection
{
public:

	FSimplyStreamCriticalSection() {}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock(void)
	{
	}

	/**
	 * Attempt to take a lock and returns whether or not a lock was taken.
	 *
	 * @return true if a lock was taken, false otherwise.
	 */
	FORCEINLINE bool TryLock()
	{
		return false;
	}

	/**
	 * Releases the lock on the critical seciton
	 */
	FORCEINLINE void Unlock(void)
	{
	}

private:
	FSimplyStreamCriticalSection(const FSimplyStreamCriticalSection&);
	FSimplyStreamCriticalSection& operator=(const FSimplyStreamCriticalSection&);
};

typedef FSimplyStreamCriticalSection FCriticalSection;
typedef FSystemWideCriticalSectionNotImplemented FSystemWideCriticalSection;
typedef TGenericPlatformRWLock<FSimplyStreamCriticalSection> FRWLock;

#endif
