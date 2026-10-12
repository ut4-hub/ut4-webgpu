#pragma once
#include "wonder_log.h"
class AssetTracker {
public:
	explicit AssetTracker()	{
	}

	void reset(uint32_t _size) {
		size = _size;
		buffer1.assign(size, false);
		buffer2.assign(size, false);
		// activeBuffer is an atomic pointer, initially points to buffer1
		activeBuffer = &buffer1;
	}

	// Only one atomic LOAD here, no atomic on the bool itself
	void setRead(uint32_t id) {
		if (id >= size) {
			wonder_log::info("Why reading over size? {} {}", id, size);
			return;
		}
		//wonder_log::info("setRead {}", id);
		auto* bufPtr = activeBuffer.load();
		(*bufPtr)[id] = true;
	}

	// Snapshot: swap buffers (atomic store/exchange once)
	std::vector<uint32_t> saveSnapshot() {
		wonder_log::info("saveSnapshot");
		auto* currentActive = activeBuffer.load(std::memory_order_acquire);
		auto* newActive = (currentActive == &buffer1) ? &buffer2 : &buffer1;
		auto* oldActive = activeBuffer.exchange(newActive, std::memory_order_acq_rel);

		// Clear the new active buffer so it's fresh
		newActive->assign(size, false);
		std::vector<uint32_t> out;
		out.reserve(size);  // reserve the maximum possible size

		for (uint32_t i = 0; i < size; ++i) {
			if ((*oldActive)[i]) {
				out.push_back(i);
			}
		}
		// Shrink to fit if necessary.
		out.shrink_to_fit();
		return out;
	}

private:
	uint32_t size = 0;
	std::vector<bool> buffer1;
	std::vector<bool> buffer2;

	// Atomic pointer to whichever buffer is active
	std::atomic<std::vector<bool>*> activeBuffer;
};