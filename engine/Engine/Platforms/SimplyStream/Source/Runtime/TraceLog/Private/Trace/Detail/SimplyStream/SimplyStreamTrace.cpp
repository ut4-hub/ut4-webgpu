// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Trace/Config.h"
#if PLATFORM_WASM && !UE_BUILD_SHIPPING
#define EMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES 0
//#undef verify
#include <emscripten.h>
#include <emscripten/val.h>
#include <emscripten/websocket.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <deque>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <exception>
#include <stdexcept>
#include <cstring>
extern CORE_API EMSCRIPTEN_WEBSOCKET_T g_websocket;
extern double global_start;
//#include "SimplyStreamPlatformTime.h"
#include "Trace/Trace.h"
#if TRACE_PRIVATE_MINIMAL_ENABLED
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <thread>
//#include "wonder_log.h"

#define USE_HP (UE_TRACE_ENABLED && 0)
#if USE_HP
std::atomic<uint64_t> highPrecisionCounter{ 0 };
uint64_t highPrecisionFrequency = 0;
#endif
namespace UE {
namespace Trace {
namespace Private {
	uint32 TotalSize = 0;
	std::string dest;
////////////////////////////////////////////////////////////////////////////////
uint8* MemoryReserve(SIZE_T Size)
{
	void* Ptr = mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	return (Ptr != MAP_FAILED) ? reinterpret_cast<uint8*>(Ptr) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
void MemoryFree(void* Address, SIZE_T Size)
{
	munmap(Address, Size);
}

////////////////////////////////////////////////////////////////////////////////
void MemoryMap(void* Address, SIZE_T Size)
{
	// no-op if mmap()ed with R/W
}

////////////////////////////////////////////////////////////////////////////////
void MemoryUnmap(void* Address, SIZE_T Size)
{
	madvise(Address, Size, MADV_DONTNEED);
}



////////////////////////////////////////////////////////////////////////////////
UPTRINT ThreadCreate(const ANSICHAR* Name, void (*Entry)())
{
	void* (*PthreadThunk)(void*) = [] (void* Param) -> void * {
		typedef void (*EntryType)(void);
		(EntryType(Param))();
		return nullptr;
	};

	pthread_t ThreadHandle;
	if (pthread_create(&ThreadHandle, nullptr, PthreadThunk, reinterpret_cast<void *>(Entry)) != 0)
	{
		return 0;
	}
	return static_cast<UPTRINT>(ThreadHandle);
}

////////////////////////////////////////////////////////////////////////////////
extern "C" {
	extern void __emscripten_atomics_sleep(double);
}
void ThreadSleep(uint32 Milliseconds)
{
	__emscripten_atomics_sleep(Milliseconds);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadJoin(UPTRINT Handle)
{
	pthread_join(static_cast<pthread_t>(Handle), nullptr);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadDestroy(UPTRINT Handle)
{
	// no-op
}
#if USE_HP

void highPrecisionCounterThread() {
	auto start = emscripten_get_now();
	while (true) {
		highPrecisionCounter++;
		if (highPrecisionCounter % 1000000 == 0) {
			auto end = emscripten_get_now();
			auto elapsed = end - start;
			highPrecisionFrequency = static_cast<uint64_t>(1000000 / (elapsed / 1000.0));
			start = end;
		}
	}
}
std::thread hpThread = std::thread(highPrecisionCounterThread);
uint64_t TimeGetFrequency() {
	while (highPrecisionFrequency == 0) {
		//wonder_log::info("Waiting for high precision frequency");
	}
	return highPrecisionFrequency;
}

uint64_t TimeGetTimestamp() {
	auto out = highPrecisionCounter.load();
	//wonder_log::info("TimeGetTimestamp: {}", out);
	return out;
}
#else
////////////////////////////////////////////////////////////////////////////////
uint64 TimeGetFrequency() {
	return 200000ull; // 1 second / 5 microseconds
}

////////////////////////////////////////////////////////////////////////////////
uint64 TimeGetTimestamp()
{
	// should stay in sync with FPlatformTime::Cycles64() or the timeline will be broken!
	return (uint64)((emscripten_get_now() - global_start) * 200.0);
}
#endif



////////////////////////////////////////////////////////////////////////////////
static bool TcpSocketSetNonBlocking(int Socket, bool bNonBlocking)
{
	int Flags = fcntl(Socket, F_GETFL, 0);
	if (Flags == -1)
	{
		return false;
	}

	Flags = bNonBlocking ? (Flags|O_NONBLOCK) : (Flags & ~O_NONBLOCK);
	return fcntl(Socket, F_SETFL, Flags) >= 0;
}

////////////////////////////////////////////////////////////////////////////////
UPTRINT TcpSocketConnect(const ANSICHAR* Host, uint16 Port)
{
	struct FAddrInfoPtr
	{
					~FAddrInfoPtr()	{ freeaddrinfo(Value); }
		addrinfo*	operator -> ()	{ return Value; }
		addrinfo**	operator & ()	{ return &Value; }
		addrinfo*	Value;
	};

	FAddrInfoPtr Info;
	addrinfo Hints = {};
	Hints.ai_family = AF_INET;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_protocol = IPPROTO_TCP;
	if (getaddrinfo(Host, nullptr, &Hints, &Info))
	{
		return 0;
	}

	if (&Info == nullptr)
	{
		return 0;
	}

	auto* SockAddr = (sockaddr_in*)Info->ai_addr;
	SockAddr->sin_port = htons(Port);

	int Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (Socket < 0)
	{
		return 0;
	}

	int Result = connect(Socket, Info->ai_addr, int(Info->ai_addrlen));
	if (Result < 0)
	{
		close(Socket);
		return 0;
	}

	if (!TcpSocketSetNonBlocking(Socket, false))
	{
		close(Socket);
		return 0;
	}

	return UPTRINT(Socket + 1);
}

////////////////////////////////////////////////////////////////////////////////
UPTRINT TcpSocketListen(uint16 Port)
{
	int Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (Socket < 0)
	{
		return 0;
	}

	sockaddr_in SockAddr;
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_addr.s_addr = 0;
	SockAddr.sin_port = htons(Port);
	int Result = bind(Socket, reinterpret_cast<sockaddr*>(&SockAddr), sizeof(SockAddr));
	if (Result < 0)
	{
		close(Socket);
		return 0;
	}

	Result = listen(Socket, 1);
	if (Result < 0)
	{
		close(Socket);
		return 0;
	}

	if (!TcpSocketSetNonBlocking(Socket, true))
	{
		close(Socket);
		return 0;
	}

	return UPTRINT(Socket + 1);
}

////////////////////////////////////////////////////////////////////////////////
int32 TcpSocketAccept(UPTRINT Socket, UPTRINT& Out)
{
	int Inner = Socket - 1;

	Inner = accept(Inner, nullptr, nullptr);
	if (Inner < 0)
	{
		return (Inner == EAGAIN || Inner == EWOULDBLOCK) - 1; // 0 if would block else -1
	}

	if (!TcpSocketSetNonBlocking(Inner, false))
	{
		close(Inner);
		return 0;
	}

	Out = UPTRINT(Inner + 1);
	return 1;
}

////////////////////////////////////////////////////////////////////////////////
bool TcpSocketHasData(UPTRINT Socket)
{
	int Inner = Socket - 1;
	fd_set FdSet;
	FD_ZERO(&FdSet);
	FD_SET(Inner, &FdSet);
	timeval TimeVal = {};
	return (select(Inner + 1, &FdSet, nullptr, nullptr, &TimeVal) != 0);
}

#ifndef KEEP_IN_MODULE
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#endif

//extern "C" void flzma_compress(std::string & to_compress, int value);
KEEP_IN_MODULE void sendTrace();

KEEP_IN_MODULE void downloadTrace2() {
	EM_ASM({
		//let content = new Blob([HEAPU8.subarray($0, $0 + $1)], { type: 'application/octet-stream' });
		let content2 = new Uint8Array(Module['wasmMemory'].buffer, $0, $1);
		let content = new Blob([content2], { type: 'application/octet-stream' });
		console.log('Downloading trace:', $1);
		var a = document.createElement('a');
		a.download = 'save.utrace';
		a.href = URL.createObjectURL(content);
		a.style.display = 'none';

		document.body.appendChild(a);
		a.click();
		setTimeout(() => {
			document.body.removeChild(a);
			URL.revokeObjectURL(a.href);
		}, 2000);
		}, dest.data(), TotalSize);
}

#if 0
#include <emscripten/bind.h>

emscripten::val getInt8Array() { return emscripten::val(emscripten::typed_memory_view(TotalSize, dest.data())); }

EMSCRIPTEN_BINDINGS() {
	function("getInt8Array", &getInt8Array);
}
#endif
extern "C" __attribute__((used, visibility("default"))) void downloadTrace3() {
	MAIN_THREAD_EM_ASM({
		let content = new Blob([new Uint8Array(HEAPU8.subarray($0, $0 + $1))], { type: 'application/octet-stream' });
		console.log(`Downloading trace: ${$1}`);
		var a = document.createElement('a');
		a.download = 'save.utrace';
		a.href = URL.createObjectURL(content);
		a.style.display = 'none';

		document.body.appendChild(a);
		a.click();
		setTimeout(() => {
			document.body.removeChild(a);
			URL.revokeObjectURL(a.href);
		}, 2000);
		}, dest.data(), TotalSize);
}
bool stop_trace = false;
////////////////////////////////////////////////////////////////////////////////
bool IoWrite(UPTRINT Handle, const void* Data, uint32 Size) {
	if (stop_trace) return true;
	if (dest.size() == 0)
		dest.resize(128 << 20);
	if (TotalSize + Size > dest.size()) {
		EM_ASM({ console.log('Error, tracing is taking too much memory.', $0); }, TotalSize);
		//dest.resize(0);
		//dest.shrink_to_fit();
		TotalSize = 0;
		stop_trace = true;
		return true;
	}
	memcpy(dest.data() + TotalSize, Data, Size);
	TotalSize += Size;
	return true;
}

////////////////////////////////////////////////////////////////////////////////
int32 IoRead(UPTRINT Handle, void* Data, uint32 Size) {
	//recv websocket commands here
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
void IoClose(UPTRINT Handle) {
}



////////////////////////////////////////////////////////////////////////////////
UPTRINT FileOpen(const ANSICHAR* Path) {
#if PLATFORM_WASM
	//EM_ASM({ console.log('FileOpen_HTML5'); });
#endif
	return UPTRINT(1);
}

////////////////////////////////////////////////////////////////////////////////
int32 GetLastErrorCode()
{
	return errno;
}

////////////////////////////////////////////////////////////////////////////////
bool GetErrorMessage(char* OutBuffer, uint32 BufferSize, int32 ErrorCode)
{
	const char* ErrorMessage = std::strerror(ErrorCode);
	const bool bResult = strncpy(OutBuffer, ErrorMessage, BufferSize) != 0;
	OutBuffer[BufferSize - 1] = 0;
	return bResult;
}

} // namespace Private
} // namespace Trace
} // namespace UE

#endif
#endif // UE_TRACE_ENABLED
