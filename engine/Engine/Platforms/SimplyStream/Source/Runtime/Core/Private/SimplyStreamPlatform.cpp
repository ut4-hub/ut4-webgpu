#include "SimplyStreamPlatform.h"
#include <emscripten.h>
#include <emscripten/threading.h>
namespace UE {
	namespace Trace {
		namespace Private {
			extern void ThreadYield() {
				constexpr uint64_t SLEEP_ITERATIONS = 20;
				static thread_local uint64_t SLEEP_ITERATIONS_i = 0;
				static uint64_t shared_atomic_wait = 0;
				constexpr double SLEEP_TIME_MS = 1.1;
				constexpr uint64_t SLEEP_TIME_NS = SLEEP_TIME_MS * 1000000;
				if (SLEEP_ITERATIONS_i++ % SLEEP_ITERATIONS == 0) {
					__builtin_wasm_memory_atomic_wait32((int*)shared_atomic_wait, 0, SLEEP_TIME_NS);
				}
			}
		}
	}
}