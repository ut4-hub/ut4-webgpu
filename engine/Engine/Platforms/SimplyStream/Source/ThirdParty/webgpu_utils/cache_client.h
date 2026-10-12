#pragma once
#include <stdint.h>
#include <list>
#include <map>
#include <mutex>
#include <vector>
#include "wasm_atomic.h"
#include "wonder_log.h"
//#include "ZSTD/Public/archive.h"
#include "zstd_cpp.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "GenericPlatform/GenericPlatform.h"

#define PROFILE_CACHE 0
extern uint32_t SimplyStreamGetSize(datahash key);
extern "C" void* simplystream_malloc(size_t size, asset_index key);
extern "C" void simplystream_free(void* ptr);
#include "AtomicQueue.h"
void SetLocalThreadLockStatus(bool status);
void SetLocalThreadStatus(datahash hash, ThreadStatus status);
void simplystream_touch(asset_index key);
ThreadStatus GetLocalThreadStatus();
#if !(UE_BUILD_SHIPPING) || PROFILE_CACHE
#define AGGRESSIVE_SCOPED_LOADTIMER(name) SCOPED_LOADTIMER(name)
#else
#define AGGRESSIVE_SCOPED_LOADTIMER(name)
#endif

enum class SessionEventType : uint8_t {
	STARTUP,
	PREFETCH,
	LOAD,
	LEVEL,
	END,
	TOUCH,
	READ,
	DECOMPRESS,
	FETCH,
	SIZE,
	EVICT,
	PRUNE,
	PRUNE_WAIT,
};
extern void emit_session_event(const SessionEventType type, const uint64_t hash, double duration_ms);

struct View {
	std::string_view data;
	Atomic<int32_t> use_count = 0;
	Atomic<uint32_t> fetch_status = 0; // 0 = not fetched, 1 = fetching, 2 = fetched, 3 = decompressing, 4 = decompressed, 5 = reseting
	asset_index index;
	uint64_t getHash();
	uint64_t getSize();
	bool reset() {
		//emit_session_event(SessionEventType::EVICT, getHash());
		if (fetch_status.load() != 4) {
			return false;
		}
		fetch_status = 5;
		fetch_status.notify();
		simplystream_free((void*)data.data());
		data = {};
		fetch_status = 0;
		fetch_status.notify();
		return true;
	}
	bool ready() {
		return fetch_status == 4;
	}
	void wait_for_ready();
	void wait_on_reseting();
	bool cond_upgrade_status_to_fetching() {
		uint32_t expected = 0;
		bool exchanged = fetch_status.compare_exchange_strong(expected, 1);
		if (exchanged) {
			fetch_status.notify();
			return true;
		}
		return false;
	}
	bool cond_upgrade_status_to_decompressing() {
		uint32_t expected = 2;
		bool exchanged = fetch_status.compare_exchange_strong(expected, 3);
		if (exchanged) {
			fetch_status.notify();
			return true;
		}
		return false;
	}
	void wait_on_fetching() {
		while (fetch_status.load() < 2) {
		}
	}
	void IncreaseRefCount() {
		use_count += 1;
	}
	void DecreaseRefCount() {
		use_count -= 1;
	}
	void set_ready() {
		fetch_status = 4;
		fetch_status.notify();
	}
	void decompress();
	void copy(void* ptr, size_t offset, size_t size);
};
class ThreadSafeList {
   public:
	void push_back(asset_index value) {
		std::lock_guard<std::mutex> lock(mutex_);
		list_.push_back(value);
		auto it = std::prev(list_.end());
		iterator_map_[value] = it;
	}

	void push_front(asset_index value) {
		std::lock_guard<std::mutex> lock(mutex_);
		list_.push_front(value);
		auto it = list_.begin();
		iterator_map_[value] = it;
	}

	void pop_back() {
		std::lock_guard<std::mutex> lock(mutex_);
		list_.pop_back();
	}

	asset_index front() {
		std::lock_guard<std::mutex> lock(mutex_);
		return list_.front();
	}

	asset_index back() {
		std::lock_guard<std::mutex> lock(mutex_);
		return list_.back();
	}

	void erase(typename std::list<asset_index>::iterator it) {
		std::lock_guard<std::mutex> lock(mutex_);
		list_.erase(it);
	}

	void pop_front() {
		std::lock_guard<std::mutex> lock(mutex_);
		list_.pop_front();
	}

	typename std::list<asset_index>::iterator begin() {
		std::lock_guard<std::mutex> lock(mutex_);
		return list_.begin();
	}

	typename std::list<asset_index>::iterator end() {
		std::lock_guard<std::mutex> lock(mutex_);
		auto _end = list_.end();
		return _end;
	}

	bool empty() {
		std::lock_guard<std::mutex> lock(mutex_);
		return list_.empty();
	}

	void erase_by_value(asset_index value) {
		std::lock_guard<std::mutex> lock(mutex_);
		auto it_map = iterator_map_.find(value);
		if (it_map != iterator_map_.end()) {
			list_.erase(it_map->second);
			iterator_map_.erase(it_map);
		}
	}

	std::vector<asset_index> get_vector() {
		std::lock_guard<std::mutex> lock(mutex_);
		return { list_.begin(), list_.end() };
	}

   private:
	std::list<asset_index> list_;
	std::mutex mutex_;
	HashT_ST<asset_index, typename std::list<asset_index>::iterator> iterator_map_ = HashT_ST<asset_index, typename std::list<asset_index>::iterator>(1000);
};
struct Cache {
	uint64_t max_mem = 800 * 1024 * 1024;
	Atomic<uint64_t> used_mem;
	std::vector<View> elements;
	ThreadSafeList list;
	Atomic<int32_t> pruning;
	void insert(asset_index key) {
		list.erase_by_value(key);
		list.push_back(key);
	}
	Cache() {
		//elements.resize(20000);
	}
	bool needs_prune() {
		int expected = false;
		int desired = true;
		if (pruning.compare_exchange_strong(expected, desired)) {
		} else {
			return false;
		}
		return true;
	}
	bool get(asset_index key, View*& value);
	void touch(asset_index key) {
		insert(key);
	}
	void fetch_range(const std::vector<asset_index>& keys) {
		View* view;
		for (auto key : keys) {
			if (key == 0) {
				continue;
			}
			get(key, view);
		}
	}
	void set(asset_index key, datahash hash, std::string_view value) {
		//AGGRESSIVE_SCOPED_LOADTIMER(set);
		// prune();
		auto& v = elements[key];
		//v.wait_on_reseting();
		v.data = value;
		v.fetch_status = 2;
		v.fetch_status.notify();
		used_mem += value.size();
		//wonder_log::info("set key {} size {} hash {}", key, value.size(), hash);
	}
	void unset_pruning() {
		//wonder_log::verbose("unset pruning");
		pruning = 0;
		pruning.notify();
	}
	int get_status(asset_index key) {
		return elements[key].ready();
	}
	void on_fetch(asset_index key) {
		elements[key].fetch_status = 2;
		elements[key].fetch_status.notify();
	}
	void prune() {
		if (used_mem < max_mem) {
			return;
		}
		AGGRESSIVE_SCOPED_LOADTIMER(prune);
		bool expected = false;
		bool newObject = true;
		auto last = GetLocalThreadStatus();
		auto _needs_prune = needs_prune();
		if (!_needs_prune) {
			//wonder_log::verbose("already pruning, just waiting");
			SetLocalThreadStatus(0, ThreadStatus::PRUNE_WAIT);
			if (pruning.wait(1, 10s) == WaitResult::TimedOut) {
				wonder_log::error("pruning taking longer than 1s");
			}
			SetLocalThreadStatus(0, last);
			return;
		}
		SetLocalThreadStatus(0, ThreadStatus::PRUNE);
		//wonder_log::verbose("prune start, used_mem {} max_mem {}", used_mem, max_mem);
		if (list.empty()) {
			wonder_log::error("need space but list empty");
			SetLocalThreadStatus(0, last);
			return;
		}
#if !(UE_BUILD_SHIPPING) || PROFILE_CACHE
		auto start = wndr_get_now();
#endif
		auto _list = list.get_vector();
#define DEBUG_PRUNE 0
#if DEBUG_PRUNE
		std::string pruned;
		int prune_count = 0;
#endif
		for (auto key : _list) {
			if (used_mem < max_mem) {
				//wonder_log::verbose("free'd enough");
				break;
			}
			auto& v = elements[key];
			auto size = v.data.size();
			if (!v.reset()) {
				continue;
			}
#if DEBUG_PRUNE
			pruned += std::to_string(key) + "\n";
			prune_count++;
			wonder_log::verbose("Pruned key {} freeing mem {}", key, size);
#endif
			list.erase_by_value(key);
			used_mem -= size;
		}
#if DEBUG_PRUNE
		if (pruned.size() > 0)
			wonder_log::verbose("pruned keys({})\n{}", prune_count, pruned);
#endif
#if !(UE_BUILD_SHIPPING) || PROFILE_CACHE
		auto end = wndr_get_now();
		if (end - start > 1)
			wonder_log::info("prune took {} ms", end - start);
#endif
		unset_pruning();
		SetLocalThreadStatus(0, last);
	}
};