#pragma once
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <atomic>
#include <functional>
#include <stddef.h>
#include <stdint.h>

#ifndef KEEP_IN_MODULE
#ifdef __EMSCRIPTEN__
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#else
#define KEEP_IN_MODULE extern "C"
#endif
#endif

#include "CoreMinimal.h"

#if UE_BUILD_TEST
#define AGGRESSIVE_STATS_SCOPE(name, color) SCOPED_NAMED_EVENT_TEXT(name, color)
#else
#define AGGRESSIVE_STATS_SCOPE(name, color)
#endif

#define UNSUPPORTED_PATH wonder_log::error("Unsupported path");

using namespace std::chrono_literals;

#if defined(__EMSCRIPTEN__)
#define wndr_get_now emscripten_get_now
#else
// Per-TU internal linkage: avoids Apple ld64's duplicate-symbol error on the
// thread-local init routine when this header is included by multiple TUs in
// the same dylib (e.g. when Adaptive Build splits a file out of unity).
static thread_local std::chrono::high_resolution_clock::time_point g_start_time = std::chrono::high_resolution_clock::now();
inline static double wndr_get_now() {
	return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::chrono::high_resolution_clock::now() - g_start_time).count();
}
#endif
