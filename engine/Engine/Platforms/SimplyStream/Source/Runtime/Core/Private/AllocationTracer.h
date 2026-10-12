#pragma once
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include <atomic>

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

// ===== Compile-time defaults (see block above) =====
#ifndef ALLOCTRACE_ENABLE
#define ALLOCTRACE_ENABLE 1
#endif
#ifndef ALLOCTRACE_SAMPLE_N
#define ALLOCTRACE_SAMPLE_N 1
#endif
#ifndef ALLOCTRACE_TRACE_STACKS
#define ALLOCTRACE_TRACE_STACKS 1
#endif
#ifndef ALLOCTRACE_TOPK
#define ALLOCTRACE_TOPK 20
#endif
#ifndef ALLOCTRACE_MINBYTES
#define ALLOCTRACE_MINBYTES 4096
#endif
#ifndef ALLOCTRACE_PRINT_INTERVAL_SEC
#define ALLOCTRACE_PRINT_INTERVAL_SEC 10
#endif
#ifndef ALLOCTRACE_STACK_CAP
#define ALLOCTRACE_STACK_CAP 2048  // bytes we keep per filtered stack
#endif
#ifndef ALLOCTRACE_ALERT_ENABLE
#define ALLOCTRACE_ALERT_ENABLE 1
#endif
#ifndef ALLOCTRACE_ALERT_STEP_BYTES
#define ALLOCTRACE_ALERT_STEP_BYTES (200u * 1024u * 1024u) // 200 MB
#endif

// ===== Reentrancy guard =====
static thread_local int32 G_AT_TraceDepth = 0;
struct FScopedTraceSuspend {
	FScopedTraceSuspend() { ++G_AT_TraceDepth; }
	~FScopedTraceSuspend() { --G_AT_TraceDepth; }
	static bool IsSuspended() { return G_AT_TraceDepth > 0; }
};

// ===== Hash / stack (UTF-8) =====
inline uint32 AT_HashUtf8(const char* S) {
	uint32 H = 2166136261u;
	for (const unsigned char* p = (const unsigned char*)S; *p; ++p) { H ^= *p; H *= 16777619u; }
	return H ? H : 1u;
}
inline const char* AT_CaptureStackUtf8() {
#if ALLOCTRACE_TRACE_STACKS && defined(__EMSCRIPTEN__)
	static char Raw[ALLOCTRACE_STACK_CAP];
	emscripten_get_callstack(EM_LOG_JS_STACK | EM_LOG_NO_PATHS, Raw, sizeof(Raw));
	if (!Raw[0]) return "";

	// any line that contains one of these substrings is “infrastructure”; skip it
	static const char* const kSkipSubs[] = {
		"FCountingMalloc::OnAlloc",
		"FCountingMalloc::Malloc",
		"FCountingMalloc::Realloc",
		"FCountingMalloc::Free",
		"FMallocThreadSafeProxy::Malloc",
		"FMallocThreadSafeProxy::Realloc",
		"FMallocThreadSafeProxy::Free",
		"FMemory::Malloc",
		"FMemory::Realloc",
		"FMemory::Free",
		"operator new",
		"operator delete"
	};

	auto ShouldSkip = [&](const char* line) -> bool {
		for (const char* sub : kSkipSubs) {
			if (FCStringAnsi::Strstr(line, sub)) return true;
		}
		return false;
		};

	// Walk lines, skipping infra until the first non-matching line
	const char* p = Raw;
	for (;;)
	{
		const char* line = p;
		if (*line == '\0') break;

		// find end of this line
		while (*p && *p != '\n') ++p;
		const bool hasNL = (*p == '\n');
		if (hasNL) *const_cast<char*>(p) = '\0'; // temporarily terminate line

		if (!ShouldSkip(line)) {
			// copy the remainder (from this first non-skipped line) to out
			thread_local char Out[ALLOCTRACE_STACK_CAP];
			size_t rem = FCStringAnsi::Strlen(line);
			if (hasNL) *const_cast<char*>(p) = '\n'; // restore char we clobbered
			// include the rest of the original buffer after this line
			const char* rest = hasNL ? (line) : line; // already at start
			// Recompute remaining length safely
			rem = FCStringAnsi::Strlen(rest);
			size_t n = FMath::Min(rem, (size_t)ALLOCTRACE_STACK_CAP - 1);
			FMemory::Memcpy(Out, rest, n);
			Out[n] = 0;
			return Out;
		}

		// restore and advance to start of next line
		if (hasNL) { *const_cast<char*>(p) = '\n'; ++p; }
	}

	// If everything matched the skip list, fall back to raw (or empty)
	return "";
#else
	return "";
#endif
}

// Small fixed buffer (no heap in hot path)
struct FTinyUtf8 {
	enum { Cap = 512 };
	char S[Cap];
	FTinyUtf8() { S[0] = 0; }
	void Set(const char* In) {
		FCStringAnsi::Strncpy(S, In ? In : "", Cap - 1);
		S[Cap - 1] = 0;
	}
	const char* Get() const { return S; }
};

// ===== Records & sharded map =====
struct FAllocRecord { uint32 Size = 0; uint32 StackHash = 0; };
struct FAT_Shard { FCriticalSection M; TMap<void*, FAllocRecord> Map; };

class FAT_Aggregator {
public:
	TMap<uint32, uint64>  LiveByStack;
	TMap<uint32, FTinyUtf8> OneStackUtf8;

	void Add(uint32 H, uint32 Size, const char* Utf8) {
		uint64& L = LiveByStack.FindOrAdd(H);
		L += Size;
		if (Utf8 && Utf8[0] && !OneStackUtf8.Contains(H)) {
			OneStackUtf8.Add(H, FTinyUtf8{}).Set(Utf8); // runs under suspend
		}
	}
	void Sub(uint32 H, uint32 Size) {
		if (uint64* L = LiveByStack.Find(H)) { *L = (*L >= Size) ? *L - Size : 0; }
	}
	void Reset() { LiveByStack.Reset(); OneStackUtf8.Reset(); }

#if ALLOCTRACE_ALERT_ENABLE
	// For each stackhash, the next byte threshold at which we should alert.
	TMap<uint32, uint64> NextAlertAt;

	void MaybeAlert(uint32 StackHash, uint64 LiveBytes, const char* OptionalUtf8)
	{
		uint64& next = NextAlertAt.FindOrAdd(StackHash, (uint64)ALLOCTRACE_ALERT_STEP_BYTES);
		if (LiveBytes >= next)
		{
#if defined(__EMSCRIPTEN__)
			// Emit a real browser console.error
			if (OptionalUtf8 && OptionalUtf8[0])
			{
				emscripten_log(EM_LOG_ERROR,
					"[alloctrace] stackhash=0x%08x live=%llu bytes (>= %llu)\\n%s",
					(unsigned)StackHash,
					(unsigned long long)LiveBytes,
					(unsigned long long)next,
					OptionalUtf8);
			}
			else
			{
				emscripten_log(EM_LOG_ERROR,
					"[alloctrace] stackhash=0x%08x live=%llu bytes (>= %llu)",
					(unsigned)StackHash,
					(unsigned long long)LiveBytes,
					(unsigned long long)next);
			}
#else
			UE_LOG(LogTemp, Error, TEXT("[alloctrace] stackhash=0x%08x live=%llu bytes (>= %llu)"),
				StackHash, (unsigned long long)LiveBytes, (unsigned long long)next);
#endif
			// Schedule the next alert at the next multiple (200MB, 400MB, …)
			next += (uint64)ALLOCTRACE_ALERT_STEP_BYTES;
		}
	}
#endif
};

// ===== Allocator wrapper =====
class FCountingMalloc final : public FMalloc {
public:
	explicit FCountingMalloc(FMalloc* InInner) : Inner(InInner) { check(Inner); }

	// Malloc family
	virtual void* Malloc(SIZE_T Size, uint32 Align) override {
#if !ALLOCTRACE_ENABLE
		return Inner->Malloc(Size, Align);
#else
		if (FScopedTraceSuspend::IsSuspended()) return Inner->Malloc(Size, Align);
		void* P = Inner->Malloc(Size, Align);
		if (P) OnAlloc(P, (uint32)Size);
		return P;
#endif
	}
	virtual void* TryMalloc(SIZE_T Size, uint32 Align) override {
#if !ALLOCTRACE_ENABLE
		return Inner->TryMalloc(Size, Align);
#else
		if (FScopedTraceSuspend::IsSuspended()) return Inner->TryMalloc(Size, Align);
		void* P = Inner->TryMalloc(Size, Align);
		if (P) OnAlloc(P, (uint32)Size);
		return P;
#endif
	}
	virtual void* Realloc(void* Ptr, SIZE_T NewSize, uint32 Align) override {
#if !ALLOCTRACE_ENABLE
		return Inner->Realloc(Ptr, NewSize, Align);
#else
		if (FScopedTraceSuspend::IsSuspended()) return Inner->Realloc(Ptr, NewSize, Align);
		if (Ptr) OnFree(Ptr);
		void* NP = Inner->Realloc(Ptr, NewSize, Align);
		if (NP) OnAlloc(NP, (uint32)NewSize);
		return NP;
#endif
	}
	virtual void* TryRealloc(void* Ptr, SIZE_T NewSize, uint32 Align) override {
#if !ALLOCTRACE_ENABLE
		return Inner->TryRealloc(Ptr, NewSize, Align);
#else
		if (FScopedTraceSuspend::IsSuspended()) return Inner->TryRealloc(Ptr, NewSize, Align);
		if (Ptr) OnFree(Ptr);
		void* NP = Inner->TryRealloc(Ptr, NewSize, Align);
		if (NP) OnAlloc(NP, (uint32)NewSize);
		return NP;
#endif
	}
	virtual void  Free(void* Ptr) override {
#if !ALLOCTRACE_ENABLE
		Inner->Free(Ptr);
#else
		if (FScopedTraceSuspend::IsSuspended()) { Inner->Free(Ptr); return; }
		OnFree(Ptr);
		Inner->Free(Ptr);
#endif
	}

	virtual bool GetAllocationSize(void* Ptr, SIZE_T& Out) override { return Inner->GetAllocationSize(Ptr, Out); }
	uint64 GetLiveBytes() const { return LiveBytes.load(std::memory_order_relaxed); }

	// Printing (call from GameThread)
	void PrintTop() {
#if ALLOCTRACE_ENABLE
		FScopedTraceSuspend Suspend;
		struct Row { uint32 H; uint64 B; };
		TArray<Row> Rows; Rows.Reserve(Agg.LiveByStack.Num());
		for (auto& KV : Agg.LiveByStack) if (KV.Value >= (uint64)ALLOCTRACE_MINBYTES) Rows.Add({ KV.Key, KV.Value });
		Rows.Sort([](const Row& A, const Row& B) { return A.B > B.B; });

		const int32 N = FMath::Min((int32)Rows.Num(), (int32)ALLOCTRACE_TOPK);
		UE_LOG(LogTemp, Warning, TEXT("=== Allocation Trace (LiveBytes=%llu) Top %d ==="),
			(unsigned long long)GetLiveBytes(), N);

		for (int32 i = 0; i < N; ++i) {
			const Row& R = Rows[i];
			if (const FTinyUtf8* Ex = Agg.OneStackUtf8.Find(R.H)) {
				UE_LOG(LogTemp, Warning, TEXT("[%d] %llu bytes @ stackhash=0x%08x\n%hs"),
					i, (unsigned long long)R.B, R.H, Ex->Get());
			}
			else {
				UE_LOG(LogTemp, Warning, TEXT("[%d] %llu bytes @ stackhash=0x%08x (no stack)"),
					i, (unsigned long long)R.B, R.H);
			}
		}
#endif
	}

	static FCountingMalloc* Instance;

private:
	FMalloc* Inner = nullptr;
	std::atomic<uint64> LiveBytes{ 0 };
#if ALLOCTRACE_ENABLE
	static constexpr uint32 NumShards = 256;
	FAT_Shard Shards[NumShards];
	FAT_Aggregator Agg;

	FORCEINLINE uint32 ShIxFor(void* Ptr) const {
		uint64 v = (uint64)(uintptr_t)Ptr;
		v ^= v >> 33; v *= 0xff51afd7ed558ccdULL;
		v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ULL;
		v ^= v >> 33; return (uint32)v & (NumShards - 1);
	}

	void OnAlloc(void* P, uint32 Size) {
		FScopedTraceSuspend Suspend; // all tracer internals run suspended
		LiveBytes.fetch_add(Size, std::memory_order_relaxed);

		static thread_local uint32 Cnt = 0;
		const uint32 N = (uint32)ALLOCTRACE_SAMPLE_N;
		const bool bTrack = (N <= 1) || ((++Cnt % N) == 0);
		if (!bTrack) return;

		uint32 H = 1;
#if ALLOCTRACE_TRACE_STACKS
		const char* Stk = AT_CaptureStackUtf8();
		H = AT_HashUtf8(Stk);
#else
		const char* Stk = "";
#endif
		const uint32 Sh = ShIxFor(P);
		{ FScopeLock L(&Shards[Sh].M); Shards[Sh].Map.Add(P, FAllocRecord{ Size, H }); }
		Agg.Add(H, Size, Stk);

#if ALLOCTRACE_ALERT_ENABLE
		if (uint64* LivePtr = Agg.LiveByStack.Find(H))
		{
			Agg.MaybeAlert(H, *LivePtr, Stk);
		}
#endif
	}

	void OnFree(void* P) {
		if (!P) return;
		FScopedTraceSuspend Suspend;
		const uint32 Sh = ShIxFor(P);
		FAllocRecord R{};
		{
			FScopeLock L(&Shards[Sh].M);
			if (FAllocRecord* F = Shards[Sh].Map.Find(P)) { R = *F; Shards[Sh].Map.Remove(P); }
			else { return; }
		}
		LiveBytes.fetch_sub(R.Size, std::memory_order_relaxed);
		Agg.Sub(R.StackHash, R.Size);
	}
#endif
};

// header-only statics
inline FCountingMalloc* FCountingMalloc::Instance = nullptr;

// ===== Public helpers =====
inline FMalloc* WrapWithAllocationTracer(FMalloc* Inner) {
#if ALLOCTRACE_ENABLE
	if (!FCountingMalloc::Instance) FCountingMalloc::Instance = new FCountingMalloc(Inner);
	return FCountingMalloc::Instance;
#else
	return Inner;
#endif
}
extern "C" __attribute__((used, visibility("default"))) void AllocTrace_PrintTop() {
#if ALLOCTRACE_ENABLE
	if (FCountingMalloc::Instance) FCountingMalloc::Instance->PrintTop();
#endif
}