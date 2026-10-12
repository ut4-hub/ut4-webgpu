// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SimplyStreamPlatformMemory.cpp: SimplyStream platform memory functions
=============================================================================*/

#include "SimplyStreamPlatformMemory.h"
#include "HAL/MallocAnsi.h"
#include "Misc/CoreStats.h"
#include "CoreGlobals.h"
#include <sys/mman.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/heap.h> // emscripten_get_sbrk_ptr / emscripten_get_heap_size (wasm "used physical" proxy)
#endif

//#include "AllocationTracer.h"

void FSimplyStreamPlatformMemory::Init()
{
	// Only allow this method to be called once
	{
		static bool bInitDone = false;
		if (bInitDone)
			return;
		bInitDone = true;
	}

	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) "),
		(double)MemoryConstants.TotalPhysical / 1024.0f / 1024.0f / 1024.0f,
		MemoryConstants.TotalPhysicalGB);
}

const FPlatformMemoryConstants& FSimplyStreamPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory stats.
		extern uint64 GTotalMemoryAvailable;
		//uint64 GTotalMemoryAvailable = 2097152000;

		MemoryConstants.TotalPhysical = GTotalMemoryAvailable;
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
	}
	MemoryConstants.PageSize = 4096;
	MemoryConstants.BinnedPageSize = 65536;
	MemoryConstants.BinnedAllocationGranularity = 4096;
	MemoryConstants.OsAllocationGranularity = 65536;

	return MemoryConstants;
}

FPlatformMemoryStats FSimplyStreamPlatformMemory::GetStats()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	FPlatformMemoryStats MemoryStats;

#if defined(__EMSCRIPTEN__)
	// wasm has no OS-level RSS query, so UsedPhysical historically reported 0 (every `stat memory` / the IAS bench
	// memory column read zero — useless for the VFS-vs-IAS comparison). Use the sbrk break instead: it is the top of
	// the allocator's claimed region in the wasm linear memory (static data + stack + heap arena that all C/UE
	// allocations funnel through — FMallocAnsi→malloc→dlmalloc/sbrk, and even our mmap(MAP_ANON) VMBs are malloc-backed
	// in emscripten). That high-water is exactly the discriminating "used physical" we want: the legacy VFS keeps an
	// ~800 MiB in-heap LRU → high break; IAS keeps its persistent cache OFF-heap in the browser Cache API → low break.
	// emscripten_get_heap_size() is the wasm memory buffer length (over-grown in big steps, ≈max — non-discriminating),
	// so it's only a ceiling, not the usage signal.
	if (uintptr_t* SbrkPtr = emscripten_get_sbrk_ptr())
	{
		static uint64 GPeakBreak = 0; // sbrk rarely returns memory to the OS, but track a true high-water regardless
		MemoryStats.UsedPhysical = (uint64)(*SbrkPtr);
		GPeakBreak = FMath::Max(GPeakBreak, MemoryStats.UsedPhysical);
		MemoryStats.PeakUsedPhysical = GPeakBreak;
	}
#endif

	MemoryStats.AvailablePhysical = MemoryConstants.TotalPhysical - MemoryStats.UsedPhysical;
	return MemoryStats;
}

FMalloc* FSimplyStreamPlatformMemory::BaseAllocator()
{
	FMalloc* Inner = new FMallocAnsi(); // typically Ansi on web
	return Inner;
	//FMalloc* Wrapped = WrapWithAllocationTracer(Inner);	return Wrapped;
}

void* FSimplyStreamPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	return FMemory::Malloc(Size); //, 16);
#if 0
	void* Ptr = malloc(Size + 16 + sizeof(void*) + sizeof(SIZE_T));
	void* Result = nullptr;
	if (Ptr)
	{
		Result = Align((uint8*)Ptr + sizeof(void*) + sizeof(SIZE_T), 16);
		*((void**)((uint8*)Result - sizeof(void*))) = Ptr;
		*((SIZE_T*)((uint8*)Result - sizeof(void*) - sizeof(SIZE_T))) = Size;
	}
#else
	auto Result = memalign(65536, Size);
#endif
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Result, Size));
	return Result;
}

void FSimplyStreamPlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
	LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));
	FMemory::Free(Ptr);
}

size_t FSimplyStreamPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment()
{
	static SIZE_T OSPageSize = FPlatformMemory::GetConstants().PageSize;
	return OSPageSize;
}

size_t FSimplyStreamPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment()
{
	static SIZE_T OSPageSize = FPlatformMemory::GetConstants().PageSize;
	return OSPageSize;
}

FSimplyStreamPlatformMemory::FPlatformVirtualMemoryBlock FSimplyStreamPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(size_t InSize, size_t InAlignment)
{
	FPlatformVirtualMemoryBlock Result;
	InSize = Align(InSize, GetVirtualSizeAlignment());
	Result.VMSizeDivVirtualSizeAlignment = InSize / GetVirtualSizeAlignment();

	size_t Alignment = FMath::Max(InAlignment, GetVirtualSizeAlignment());
	check(Alignment <= GetVirtualSizeAlignment());

	Result.Ptr = mmap(nullptr, Result.GetActualSize(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (!LIKELY(Result.Ptr != MAP_FAILED))
	{
		FPlatformMemory::OnOutOfMemory(Result.GetActualSize(), InAlignment);
	}
	check(Result.Ptr && IsAligned(Result.Ptr, Alignment));
	return Result;
}



void FSimplyStreamPlatformMemory::FPlatformVirtualMemoryBlock::FreeVirtual()
{
	if (Ptr)
	{
		check(VMSizeDivVirtualSizeAlignment > 0);
		if (munmap(Ptr, GetActualSize()) != 0)
		{
			// we can ran out of VMAs here
			FPlatformMemory::OnOutOfMemory(GetActualSize(), 0);
			// unreachable
		}
		Ptr = nullptr;
		VMSizeDivVirtualSizeAlignment = 0;
	}
}

void FSimplyStreamPlatformMemory::FPlatformVirtualMemoryBlock::Commit(size_t InOffset, size_t InSize)
{
	check(IsAligned(InOffset, GetCommitAlignment()) && IsAligned(InSize, GetCommitAlignment()));
	check(InOffset >= 0 && InSize >= 0 && InOffset + InSize <= GetActualSize() && Ptr);
}

void FSimplyStreamPlatformMemory::FPlatformVirtualMemoryBlock::Decommit(size_t InOffset, size_t InSize)
{
	check(IsAligned(InOffset, GetCommitAlignment()) && IsAligned(InSize, GetCommitAlignment()));
	check(InOffset >= 0 && InSize >= 0 && InOffset + InSize <= GetActualSize() && Ptr);
	if (madvise(((uint8*)Ptr) + InOffset, InSize, MADV_DONTNEED) != 0)
	{
		// we can ran out of VMAs here too!
		FPlatformMemory::OnOutOfMemory(InSize, 0);
	}
}

/**
* LLM uses these low level functions (LLMAlloc and LLMFree) to allocate memory. It grabs
* the function pointers by calling FPlatformMemory::GetLLMAllocFunctions. If these functions
* are not implemented GetLLMAllocFunctions should return false and LLM will be disabled.
*/

#if ENABLE_LOW_LEVEL_MEM_TRACKER

int64 LLMMallocTotal = 0;

void* LLMAlloc(size_t Size)
{
	void* Ptr = FMemory::Malloc(Size);

	LLMMallocTotal += Size;

	return Ptr;
}

void LLMFree(void* Addr, size_t Size)
{
	LLMMallocTotal -= Size;
	FMemory::Free(Addr);
}

#endif
bool FSimplyStreamPlatformMemory::GetLLMAllocFunctions(void* (*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment)
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	OutAllocFunction = LLMAlloc;
	OutFreeFunction = LLMFree;
	OutAlignment = FPlatformMemory::GetConstants().PageSize;
	return true;
#else
	return false;
#endif
}
