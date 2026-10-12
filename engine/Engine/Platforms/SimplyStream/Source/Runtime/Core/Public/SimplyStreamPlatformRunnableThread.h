// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	SimplyStreamPlatformRunnableThread.h: SimplyStream platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

#ifdef __EMSCRIPTEN_PTHREADS__

#include <pthread.h>
#include <emscripten/threading.h>
#include <emscripten/stack.h> // WNDR WORKERSTK: per-thread stack bounds
#include <stdio.h>
#include "Containers/StringConv.h"
#include "Runtime/Core/Private/HAL/PThreadRunnableThread.h"

/**
 * SimplyStream implementation of the Pthreads
 */
class FSimplyStreamRunnablePThread
	: public FRunnableThreadPThread
{
protected:

	virtual ~FSimplyStreamRunnablePThread()
	{
		// Call the parent destructor body before the parent does it - see comment on that function for explanation why.
		FRunnableThreadPThread_DestructorBody();
	}

	// FRunnableThreadPThread interface

	virtual int CreateThreadWithName(pthread_t* HandlePtr, pthread_attr_t* AttrPtr, PthreadEntryPoint Proc, void* Arg, const ANSICHAR* Name) override
	{
		size_t WndrAttrStk = 0; if (AttrPtr) { pthread_attr_getstacksize(AttrPtr, &WndrAttrStk); }
		printf("WNDR WORKERSTK name=%s attr_stacksize=%zu\n", Name ? Name : "(null)", WndrAttrStk); fflush(stdout);
		int rc = pthread_create(HandlePtr, AttrPtr, Proc, Arg);
		if (rc == 0)
		{
			emscripten_set_thread_name(*HandlePtr, Name);
			// WNDR: log the pthread handle so a faulting "Pthread 0x... sent an error"
			// can be mapped back to its thread name.
			printf("WNDR WORKERSTK name=%s pthread=%p attr_stacksize=%zu\n", Name ? Name : "(null)", (void*)*HandlePtr, WndrAttrStk); fflush(stdout);
		}
		return rc;
	}

	

	// STACKFIX (2026-08-18 worker stack-overflow ~11s in): UE ALWAYS passes an
	// explicit per-thread stack size to pthread_attr_setstacksize
	// (PThreadRunnableThread.h CreateInternal), so the link-time
	// -sDEFAULT_PTHREAD_STACK_SIZE never applies to UE threads — several are
	// created with 1MB or less, and the OnlineSubsystem/module-load init chain
	// overflowed one (checkStackCookie abort in the worker entry point). wasm
	// thread stacks are plain heap allocations and only a handful of threads
	// run under -onethread, so clamp every UE thread to at least 8MB.
	virtual uint32 AdjustStackSize(uint32 InStackSize) override
	{
		InStackSize = FRunnableThreadPThread::AdjustStackSize(InStackSize);
		return FMath::Max<uint32>(InStackSize, 8u * 1024u * 1024u);
	}
};

#else // #ifdef __EMSCRIPTEN_PTHREADS__

#include "HAL/RunnableThread.h"

/**
 * @todo html5 threads: Dummy thread class
 */
class FSimplyStreamRunnableThread : public FRunnableThread
{
public:

	virtual void SetThreadPriority (EThreadPriority NewPriority)
	{

	}

	virtual void Suspend (bool bShouldPause = 1)
	{

	}
	virtual bool Kill (bool bShouldWait = false)
	{
		return false;
	}

	virtual void WaitForCompletion ()
	{

	}

public:


	/**
	 * Virtual destructor
	 */
	virtual ~FSimplyStreamRunnableThread ()
	{

	}


protected:

	virtual bool CreateInternal (FRunnable* InRunnable, const TCHAR* InThreadName,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0)
	{
		return false;
	}
};

#endif // #else // #if __EMSCRIPTEN_PTHREADS__
