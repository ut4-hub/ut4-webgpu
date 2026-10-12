#include <atomic>
#include <string.h>
#include <thread>
#include <vector>
#include <emscripten.h>
#include <emscripten/threading.h>
#include "AtomicQueue.h"
#define PROFILING 0
#define ELEMENT std::function<void*(int)>
//using Queue = atomic_queue2::AtomicQueue2<ELEMENT, 65536 / 8, true, true, false, false>; // Use heap-allocated buffer.
#define WEBGPU_EXEC(code, ...) 	EM_ASM(console.log('push')); q.push([& __VA_ARGS__](char i) -> void*{ code return nullptr; });
#define WEBGPU_WAIT
#define WEBGPU_YIELD WEBGPU_WAIT yieldToJS();