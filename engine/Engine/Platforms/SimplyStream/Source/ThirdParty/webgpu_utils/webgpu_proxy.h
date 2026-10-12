#pragma once
#include "../wasm_utils/wndr_js_abi.h"
#include "wasm_utils.h"
#include "wndr_probe.h"
#include <cstdio> // watchdog fprintf (wonder_log.h collides with the wasm_utils duplicate)
#include "wasm_atomic.h"
#include <mutex>
#include <AtomicQueue.h>

// wndr 5.8: UE 5.8 sets PLATFORM_CACHE_LINE_SIZE=128 on Apple Silicon (5.7 was 64). AtomicQueueB2's State is
// atomic<uint8_t>, so STATES_PER_CACHE_LINE = 128/1 = 128, which lands on atomic_queue::details::
// GetCacheLineIndexBits<128> — and the engine's AtomicQueue.h only specializes up to 64, so it falls to the
// primary template (value=0) and `static_assert(SHUFFLE_BITS)` fires on the Mac host build. (wasm's cache
// line is 64, so it picks GetCacheLineIndexBits<64>=6 and compiles fine — that's why only the native/host
// build broke in 5.8.) The engine header is un-editable, so supply the missing log2(128)=7 specialization
// here, before AtomicQueueB2<Task> is instantiated below. Only our proxy instantiates B2 with a 1-byte State,
// so there is no conflicting instantiation elsewhere.
namespace atomic_queue { namespace details {
	template<> struct GetCacheLineIndexBits<128> { static int constexpr value = 7; };
} } // namespace atomic_queue::details

#include "webgpu_defines.h"
#include "HAL/IConsoleManager.h"

#if UE_EDITOR
#pragma optimize("", off)
#endif

template <typename T>
using HeapMethodQueue = atomic_queue::AtomicQueueB2<T, std::allocator<T>, true /* true = more throughput, false = less cpu usage*/, true, false>;

template<typename T>
struct ArrayHolder {
	T* ptr;
	size_t length;
	ArrayHolder() : ptr(nullptr), length(0) {}
	ArrayHolder(T* p, size_t l) : ptr(p), length(l) {}
	operator T* () const {
		return ptr;
	}
};

using u32v = std::array<uint32, 8>;
// wndr-todo-start: remove this
extern uint32 _usage;
extern char* usage_buffer;
constexpr size_t usage_buffer_size = 0x1000000;
WEBGPU_UTILS_API void* alloc_usage(size_t size);
// wndr-todo-end: remove this

// wndr-note: get_memory works but is not safe and can be made faster by using thread_local + consumer position. Seperate implementation ready just needs integration
WEBGPU_UTILS_API void ResetDeepcopyRingbuffer();
WEBGPU_UTILS_API uint8_t* get_memory(uint64_t size, uint64_t alignment = 16);

constexpr uint32 SmallMallocThreshold = 1 * 1024 * 1024;
static void* small_malloc(uint32 size, uint64_t alignment = 16) {
	if (size > SmallMallocThreshold) {
		void* mem = malloc(size + 15 + sizeof(void*));
		if (!mem) return mem;
		void* ptr = (void*)(((uintptr_t)mem + 15 + sizeof(void*)) & ~(uintptr_t)15);
		((void**)ptr)[-1] = mem;
		return ptr;
	}
	return get_memory(size, alignment);
}
static void small_free(void* ptr) {
	if (!ptr) return;
	free(((void**)ptr)[-1]);
}

#define MY_Q_EM_ASM_0(q, code) ([&]() -> int {auto waitId = q.getWaitId(); q.method_queue.push([waitId, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId(($0),$1); })(); }, qPtr, waitId); return 1; });return waitId;} ())
#define MY_Q_EM_ASM_1(q, code, ...) ([&]() -> int {auto waitId = q.getWaitId(); q.method_queue.push([&, waitId, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId(($1),$2); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())
#define MY_Q_EM_ASM_2(q, code, ...) ([&]() -> int {auto waitId = q.getWaitId(); q.method_queue.push([&, waitId, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId(($2),$3); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())
#define MY_Q_EM_ASM_3(q, code, ...) ([&]() -> int {auto waitId = q.getWaitId(); q.method_queue.push([&, waitId, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId(($3),$4); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())
#define MY_Q_EM_ASM_4(q, code, ...) ([&]() -> int {auto waitId = q.getWaitId(); q.method_queue.push([&, waitId, qPtr = &q]() { EM_ASM({ (async() => { code; _unwaitId(($4),$5); })(); }, __VA_ARGS__, qPtr, waitId); return 1; });return waitId;} ())

#define GET_FIRST_ARG(_0, ...) _0
#define GET_FIRST_ARG_OF_VA_ARGS(...) GET_FIRST_ARG(__VA_ARGS__, dummy)

#define GET_MY_Q_EM_ASM(_0,_1,_2,_3,_4,_5,NAME,...) NAME
#define EM_QUEUE_ASM_ASYNC(q, code, ...) q.method_queue.push([&, qPtr = &q]() { EM_ASM({ (async() => { code; })(); }, __VA_ARGS__); return 0; });
#define EM_QUEUE_ASM_SYNC(...) {const auto waitId = GET_MY_Q_EM_ASM(__VA_ARGS__, MY_Q_EM_ASM_4, MY_Q_EM_ASM_3, MY_Q_EM_ASM_2, MY_Q_EM_ASM_1, MY_Q_EM_ASM_0)(__VA_ARGS__); GET_FIRST_ARG_OF_VA_ARGS(__VA_ARGS__).waitOnId(waitId); }
#define EM_QUEUE_ASM(...) GET_MY_Q_EM_ASM(__VA_ARGS__, MY_Q_EM_ASM_4, MY_Q_EM_ASM_3, MY_Q_EM_ASM_2, MY_Q_EM_ASM_1, MY_Q_EM_ASM_0)(__VA_ARGS__);

#define DEBUG_RESOURCE 0 // perf hog
#define WGPU_ENABLE_RESOURCE_COUNTERS (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) && 0) // perf hog
#define WEBGPU_ALLOW_WAIT_ON_ALL !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && ((PLATFORM_WINDOWS || PLATFORM_LINUX) && 1) // perf hog
#define WEBGPU_USE_REF_SYSTEM 0
#define DISABLE_ALL_PROFILING 0
#define ENABLE_VERBOSE_PROFILING 0
#define ENABLE_VERBOSE_ERRORS 0

#undef PUSH_WEBGPU
#include "webgpu_override.h"

using namespace std::chrono_literals;

enum class WndrProxyType { None, Method, Remote };
enum class WndrProxyMode { Async, Sync };
enum class WndrProxyAPI { WebGPU };

template <auto N>
struct string_litteral {
	constexpr string_litteral(const char(&str)[N]) {
		std::copy(str, str + N, value);
	}
	char value[N];
};

enum class BaseID : uint16 {
	OTHER_FUNCS(ENUM_COMMA)
	COUNT
};

#define METHOD_ELEMENT ArrayQueue::Task

#if DEBUG_RESOURCE
enum class DEBUG_TYPE {
	GET,
	GET_FAILED,
	GET_EMPTY,
	GET_UNALLOCATED,
	GET_WAIT_FAILED,
	GET_WAIT_TIMEDOUT,
	GET_WAIT_EMPTY,
	GET_WAIT_UNALLOCATED,
	ALLOCATE,
	ALLOCATE_INVALID,
	ALLOCATE_NONEMPTY,
	SET,
	SET_INVALID,
	SET_NOT_ALLOCATED,
	SET_ALREADY_ALLOCATED,
	FREE,
	REF,
	UNREF,
	WHY_UNREF,
	LOCK,
	UNLOCK,
	END_OF_FRAME,
	COUNT
};
CORE_API void add_debug_resource(DEBUG_TYPE type, uint64_t id, uint64_t val);
CORE_API void flush_debug_resources(uint64_t id_to_print = 0);
#define DEBUG_RESOURCE_VERBOSE(type, id) add_debug_resource(type, (uint64_t)id, 0)
#define DEBUG_RESOURCE_VERBOSE2(type, id, val) add_debug_resource(type, (uint64_t)id, (uint64_t)val)
#define DEBUG_RESOURCE_ERROR(cond, type, id) if(cond){add_debug_resource(type, (uint64_t)id, 0); flush_debug_resources((uint64_t)id);}
#else
#define DEBUG_RESOURCE_VERBOSE(type, id)
#define DEBUG_RESOURCE_VERBOSE2(type, id, val)
#define DEBUG_RESOURCE_ERROR(cond, type, id)
#endif

#define WNDR_PROCESSARG(StructType) \
	[[nodiscard]] static FORCEINLINE StructType ProcessArg(StructType arg) { \
		if (arg==0) return 0; /*This is not ideal, but we currently pass things to ProcessArg that can safely return 0*/ \
		auto res = IDPool<StructType>::get().getResource(arg); \
		/*checkf(res != 0, TEXT("ProcessArg failed to get resource for %s with id %u"), TEXT(#StructType), (uint64)arg);*/ \
		if (res == 0) { wonder_log::error("ProcessArg failed to get resource for {} with id {}", #StructType, (uint64)arg); return 0; } \
		return static_cast<StructType>(res); \
	}
#define WNDR_PROCESSARG_PASSTHROUGH(StructType) \
	[[nodiscard]] static FORCEINLINE StructType ProcessArg(StructType arg) { \
		return arg; \
	} \
	static FORCEINLINE StructType* copyToMemory(StructType* arg) { \
		return arg; \
	}
#define WNDR_COPYARG(StructType) \
	static FORCEINLINE StructType copyToMemory(StructType arg) { \
		return arg; \
	}

// start of copyable struct - move into deepcopy.h?

template<typename T, typename = void>
struct has_copyToMemory : std::false_type {};
template<typename T>
struct has_copyToMemory<T, std::void_t<decltype(copyToMemory(std::declval<T>()))>> : std::true_type {};

template<typename T, typename = void>
struct has_process_arg : std::false_type {};
template<typename T>
struct has_process_arg<T, std::void_t<decltype(ProcessArg(std::declval<T>()))>> : std::true_type {};

template<typename T, typename = void>
struct has_process_argp : std::false_type {};
template<typename T>
struct has_process_argp<T*, std::void_t<decltype(ProcessArg(std::declval<T*>()))>> : std::true_type {};

template<typename T>
[[nodiscard]] static FORCEINLINE T* ProcessArg(T* source);
template<typename T>
[[nodiscard]] static FORCEINLINE T ProcessArg(T source);

template<typename T>
static FORCEINLINE ArrayHolder<T>* ProcessArg(ArrayHolder<T>* source) {
	using NonConst = std::remove_const_t<T>;
	NonConst* ptr = (NonConst*)source->ptr;
	for (size_t i = 0; i < source->length; i++) {
		ptr[i] = ProcessArg(ptr[i]);
	}
	return source;
}


template<typename Arg>
static FORCEINLINE Arg CopyArg(Arg&& in) {
	if constexpr (std::is_const_v<std::remove_reference_t<Arg>>) {
		return CopyArg(const_cast<std::remove_const_t<std::remove_reference_t<Arg>>&>(in));
	}
	// everything that reaches this is just copied
	return in;
}

template<typename Arg>
static FORCEINLINE Arg copyToMemory(Arg&& in) {
	if constexpr (std::is_arithmetic_v<std::remove_reference_t<Arg>> || std::is_enum_v<std::remove_pointer_t<Arg>> || !std::is_pointer_v<Arg>) {
		return in;
	}
	else {
		static_assert(has_copyToMemory<Arg>::value, "CopyArg specialization for the given type T is not implemented.");
		return in;
	}
}

template<typename T>
static FORCEINLINE const T* copyToMemory(const T* source) {
	if constexpr (std::is_arithmetic_v<T>) {
		return source;
	}
	else {
		static_assert(has_copyToMemory<T>::value, "copyToMemory specialization for the given type T is not implemented.");
		return nullptr;
	}
}

template<typename T>
static FORCEINLINE ArrayHolder<T>* copyToMemory(const ArrayHolder<T>*& source) {
	auto dest = new (get_memory(sizeof(ArrayHolder<T>)))ArrayHolder<T>();
	dest->length = source->length;
	dest->ptr = (T*)get_memory(source->length * sizeof(*source->ptr));
	using NonConst = std::remove_const_t<T>;
	NonConst* ptr = (NonConst*)dest->ptr;
	for (size_t i = 0; i < source->length; ++i) {
		ptr[i] = copyToMemory(source->ptr[i]);
	}
	return dest;
}

template<typename T>
static FORCEINLINE ArrayHolder<T>& copyToMemory(ArrayHolder<T>& source) {
	auto dest = new (get_memory(sizeof(ArrayHolder<T>)))ArrayHolder<T>();
	dest->length = source.length;
	dest->ptr = (T*)get_memory(source.length * sizeof(*source.ptr));
	using NonConst = std::remove_const_t<T>;
	NonConst* ptr = (NonConst*)dest->ptr;
	for (size_t i = 0; i < source.length; ++i) {
		ptr[i] = copyToMemory(source.ptr[i]);
	}
	return *dest;
}
template<>
FORCEINLINE const char* copyToMemory(const char* source) {
	if (!source) { return nullptr; }
	size_t len = strlen(source) + 1; // include null terminator
	const char* destination = (const char*)get_memory(len);
	memcpy((void*)destination, source, len);
	return destination;
}

template<typename First, typename Second, typename... Rest>
static FORCEINLINE void processCopyPairs(First&& first, Second&& second, Rest&&... rest) {
	first = copyToMemory(std::forward<Second>(second));

	if constexpr (sizeof...(Rest) > 0) {
		processCopyPairs(std::forward<Rest>(rest)...);
	}
}

template<typename T2>
static FORCEINLINE void processArg(T2&& value) {
	value = ProcessArg(std::forward<T2>(value));
}

template<typename T>
void processArg(ArrayHolder<T>* a);

template<typename First, typename Second, typename... Rest>
static FORCEINLINE void processArgPairs(First&& first, Second&& second, Rest&&... rest) {
	processArg(std::forward<Second>(second));

	if constexpr (sizeof...(Rest) > 0) {
		processArgPairs(std::forward<Rest>(rest)...);
	}
}

template<typename T, typename... Args>
static FORCEINLINE void copyArgs(Args&&... args) {
	processCopyPairs(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
static FORCEINLINE void processArgs(Args&&... args) {
	processArgPairs(std::forward<Args>(args)...);
}

template<typename T>
static FORCEINLINE void copyArgs() {
}

template<typename T>
static FORCEINLINE void processArgs() {
}

#define FIELD(n) dest->n, source->n
#define FIELD_AS(n, as) dest->n, source->n
// wndr-todo: this should be using getMemory should it not?
#define ARRAY(n, c) [&]() -> ArrayHolder<std::remove_const_t<std::remove_pointer_t<decltype(dest->n)>>>* { if constexpr(type == 0){return allocate_new_array<std::remove_const_t<std::remove_pointer_t<decltype(dest->n)>>>(dest->n, dest->c, source->n, source->c);} else if constexpr(type == 4) { return allocate_new_source_array_inplace(dest->n, dest->c); } else return 0;}(), \
[&]() -> ArrayHolder<std::remove_const_t<std::remove_pointer_t<decltype(source->n)>>>* { \
		return allocate_new_source_array_inplace(source->n, source->c); \
	}()

#define MAKE_COPYABLE_STRUCT_NO_SERIALIZE(StructType, ...) \
static void deep_copy_into(StructType* dest, const StructType* source) { \
	constexpr int type = 0; \
	copyArgs<StructType>(__VA_ARGS__); \
} \
static const StructType& copyToMemory(const StructType& rsource) { \
	constexpr int type = 0; \
	auto* source = &rsource; \
	auto dest = new (get_memory(sizeof(StructType))) StructType(); \
	copyArgs<StructType>(__VA_ARGS__); \
	return *dest; \
} \
template<> \
const StructType* copyToMemory(const StructType* source) { \
if (!source) { return nullptr; } \
constexpr int type = 0; \
auto dest = new (get_memory(sizeof(StructType))) StructType(); \
copyArgs<StructType>(__VA_ARGS__); \
return dest; \
} \
template<> \
[[nodiscard]] StructType* ProcessArg(StructType* source) { \
if (!source) { return nullptr; } \
constexpr int type = 1; \
StructType* dest = source; \
processArgs<StructType>(__VA_ARGS__); \
return source; \
} \
template<> \
[[nodiscard]] const StructType* ProcessArg(const StructType* csource) { \
return ProcessArg((StructType*)csource); \
}
#define MAKE_COPYABLE_STRUCT(StructType, ...) \
MAKE_COPYABLE_STRUCT_NO_SERIALIZE(StructType, __VA_ARGS__)

template<typename T>
static const ArrayHolder<T>* copyToMemory(const ArrayHolder<T>& _source) {
	const ArrayHolder<T>* source = &_source;
	auto dest = new (get_memory(sizeof(ArrayHolder<T>)))ArrayHolder<T>();
	copyArrayToMemory(dest, source);
	return dest;
}
template<typename T>
static ArrayHolder<T>* allocate_new_array_inplace(const T* data, size_t size) {
	if (size > 0 && !data) {
		return nullptr;
	}
	auto dest = new (get_memory(sizeof(ArrayHolder<T>)))ArrayHolder<T>();
	dest->ptr = (T*)get_memory(size * sizeof(T));
	using NonConstT = std::remove_const_t<T>;
	NonConstT* ptr = (NonConstT*)dest->ptr;
	for (size_t i = 0; i < size; ++i) {
		if constexpr (std::is_class_v<T> && !std::is_trivially_copyable_v<T>) {
			new (&ptr[i]) NonConstT(); // placement new
			deep_copy_into(&ptr[i], &data[i]);
		}
		else {
			ptr[i] = copyToMemory(data[i]);
		}
	}
	dest->length = size;
	return dest;
}
template<typename T>
static ArrayHolder<T>* allocate_new_source_array_inplace(const T* data, size_t size) {
	if (!data) return nullptr;
	auto dest = new (get_memory(sizeof(ArrayHolder<T>)))ArrayHolder<T>();
	dest->ptr = (T*)data;
	dest->length = size;
	return dest;
}
template<typename T>
static ArrayHolder<T>* allocate_new_array(const T*& out, size_t& out_size, const T* data, size_t size) {
	auto dest = allocate_new_array_inplace(data, size);
	out = (T*)dest->ptr;
	out_size = dest->length;
	return dest;
}

template<typename T>
[[nodiscard]] static T* ProcessArg(T* source) {
	if constexpr (std::is_arithmetic_v<std::remove_pointer_t<T>> || std::is_enum_v<std::remove_pointer_t<T>> || std::is_void_v<T>) {
		return source;
	}
	else {
		static_assert(has_process_argp<T>::value, "ProcessArg specialization for the given type T is not implemented.");
		return source;
	}
}
template<typename T>
[[nodiscard]] static T ProcessArg(T source) {
	// if not a pointer, we don't need to process it
	if constexpr (std::is_arithmetic_v<std::remove_pointer_t<T>> || std::is_enum_v<std::remove_pointer_t<T>>) {
		return source;
	}
	else if constexpr (has_process_arg<T>::value) {
		return *ProcessArg(&source);
	}
	else {
		static_assert(has_process_arg<T>::value || has_process_argp<T>::value, "ProcessArg specialization for the given type T is not implemented.");
		return source;
	}
}

template<typename T>
void processArg(ArrayHolder<T>* a) {
	for (size_t i = 0; i < a->length; i++) {
		a->ptr[i] = ProcessArg(a->ptr[i]);
	}
}

template<typename ...Args>
auto copyAllToMemory(Args&&... args) {
	using InputTuple = std::tuple<std::remove_reference_t<Args>...>;
	using StoredTuple = std::tuple<std::remove_reference_t<decltype(copyToMemory(std::forward<Args>(args)))>...>;
	static_assert(sizeof(InputTuple) == sizeof(StoredTuple), "WebGPU proxy input and stored tuple sizes must match");
	static_assert(alignof(InputTuple) == alignof(StoredTuple), "WebGPU proxy input and stored tuple alignments must match");
	// Args preserves lvalue references from Send(). On wasm32 a tuple of those references stores
	// four-byte addresses, while StoredTuple owns copied uint64 arguments that still need eight
	// bytes. Allocating sizeof(std::tuple<Args...>) therefore under-allocated mixed pointer/u64
	// WebGPU calls (for example copyBufferToBuffer) and let the next ring allocation overwrite the
	// tail of the task. Allocate and align the object we actually construct.
	void* destination = static_cast<void*>(get_memory(sizeof(StoredTuple), alignof(StoredTuple)));
	return new (destination) StoredTuple(copyToMemory(std::forward<Args>(args))...);
}

template<typename Func>
struct function_traits : public function_traits<decltype(&Func::operator())> {};

template<typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) const> {
	using return_type = ReturnType;
};

template<typename ReturnType, typename... Args>
struct function_traits<ReturnType(*)(Args...)> {
	using return_type = ReturnType;
};

template<typename Func>
using func_return_t = typename function_traits<Func>::return_type;
#if WGPU_ENABLE_RESOURCE_COUNTERS
struct CounterStorageInternal {
	std::string name;
	std::atomic<uint64_t> count{ 0 };
	bool Registered = false;
	static std::atomic<uint64_t> all_counters_id;
	static std::vector<CounterStorageInternal*> all_counters;
};
static FORCEINLINE_DEBUGGABLE std::string clean_type_name(const std::string& name) {
	std::string result = name;

	// Remove common prefixes and suffixes
	const std::vector<std::string> to_remove = {
		"struct ", "Impl", "class ", " * __ptr64"
	};

	for (const auto& str : to_remove) {
		size_t pos = result.find(str);
		if (pos != std::string::npos) {
			result.erase(pos, str.length());
		}
	}

	return result;
}
template<typename T>
struct CounterStorage {
public:
	static CounterStorageInternal self;
	static void IncreaseCount() {
		if (!self.Registered) {
			self.Registered = true;
#if PLATFORM_WINDOWS
			self.name = clean_type_name(typeid(T).name());
#endif
			CounterStorageInternal::all_counters[CounterStorageInternal::all_counters_id++] = &self;
		}
		self.count++;
	}
	static void DecreaseCount() {
		self.count--;
	}
};
template<typename T>
CounterStorageInternal CounterStorage<T>::self{};
#define INCREASE_COUNTER(T) CounterStorage<T>::IncreaseCount()
#define DECREASE_COUNTER(T) CounterStorage<T>::DecreaseCount()
#else
#define INCREASE_COUNTER(T)
#define DECREASE_COUNTER(T)
#endif
#if 1
template<typename T>
struct MaxIDsTraits {
	static constexpr bool has_specialization = false;
	static constexpr size_t value = 2; // Default value if no specialization exists
};
template<typename T>
static constexpr inline size_t max_ids_for_type() { return MaxIDsTraits<T>::value; }
#define MAX_IDS_FOR_TYPE(T, n) \
template<> \
inline constexpr bool MaxIDsTraits<T>::has_specialization = true; \
template<> \
inline constexpr size_t MaxIDsTraits<T>::value = n+1;
#else
constexpr size_t max_ids_for_type(...) = delete; // Prevents usage without a valid overload
#define MAX_IDS_FOR_TYPE(T, n) constexpr size_t max_ids_for_type(T) { return n; }
#endif
static constexpr uint32 ClampValue(uint32 value) {
	return (value < 1) ? 1 : ((value > 200) ? 200 : value);
}
// Resource values cross pointer-typed proxy APIs before returning to this uint64
// table. Keep the reserved values pointer-width so they survive that round trip
// on native wasm32 instead of becoming zero-extended 0x00000000fffffff* values.
static constexpr uint64_t ALLOC_UNSET = static_cast<uint64_t>(UINTPTR_MAX) - 10;
static constexpr uint64_t ALLOC_FAILED = static_cast<uint64_t>(UINTPTR_MAX) - 11;
static_assert(static_cast<uint64_t>(static_cast<uintptr_t>(ALLOC_UNSET)) == ALLOC_UNSET);
static_assert(static_cast<uint64_t>(static_cast<uintptr_t>(ALLOC_FAILED)) == ALLOC_FAILED);
template<typename T>
class IDPool {
public:
	static constexpr uint32 MAX_IDS = max_ids_for_type<T>();
private:
	// assert if no specialization is found for max_ids_for_type
	static_assert(MaxIDsTraits<T>::has_specialization, "No max_ids_for_type specialization found for this type.");
	Atomic<uint32> counter;
	static inline uint32* globalFreeList = new uint32[MAX_IDS];
	uint32 globalFreeList_count{ 0 };
	std::mutex freeListMutex;
	static constexpr uint32 localMax = ClampValue(MAX_IDS / 1000);
	static thread_local inline uint32 localFreeList[localMax];
	static thread_local inline uint32 localFreeList_count = 0;
	// dont need atomic, should combine these
	// probably change uint64_t to size_t
	static inline Atomic<uint64_t>* res_array = new Atomic<uint64_t>[MAX_IDS];
public:
	static IDPool& get() {
		static IDPool instance; // Guaranteed to be thread-safe in C++11 and later
		return instance;
	}
	IDPool(const IDPool&) = delete;
	IDPool& operator=(const IDPool&) = delete;
	IDPool() {
		for (uint32 i = 0; i < MAX_IDS; i++) {
			res_array[i].store(0);
			globalFreeList[i] = 0;
		}
	}
	~IDPool() {
		for (uint32 i = 0; i < MAX_IDS; i++) {
			if (res_array[i].load() != 0) {
				//wonder_log::error("IDPool not empty on destruction");
			}
		}
	}
	bool allocate(uint32& id) {
#if DEBUG_RESOURCE
		id = counter.fetch_add(1) + 1;
		if (id < MAX_IDS) {
			return true;
		}
#endif
		// First, try to get an ID from the local free list
		if (localFreeList_count > 0) {
			id = localFreeList[--localFreeList_count];
			return true;
		}

		// Next, try the global free list
		freeListMutex.lock();
		if (globalFreeList_count > 0) {
			// batch copy to localFreeList up to localMax
			uint32 count = globalFreeList_count > localMax ? localMax : globalFreeList_count;
			localFreeList_count = count;
			for (uint32 i = 0; i < count; ++i) {
				localFreeList[i] = globalFreeList[--globalFreeList_count];
			}
			id = localFreeList[--localFreeList_count];
			freeListMutex.unlock();
			return true;
		}
		freeListMutex.unlock();

#if DEBUG_RESOURCE
		checkNoEntry();
		return false;
#endif
		// If all free lists are empty, get a new ID from the counter
		id = counter.fetch_add(1) + 1;
		if (id >= MAX_IDS) {
			checkNoEntry();
			//wonder_log::fatal("IDPool ran out of ids");
			return false;
		}
		return true;
	}

	void deallocate(uint32 id) {
		if (localFreeList_count < localMax) {
			localFreeList[localFreeList_count++] = id;
			return;
		}

		// If the local list is full, transfer its contents to the global list
		freeListMutex.lock();
		uint32 startIndex = globalFreeList_count;
		globalFreeList_count += localFreeList_count + 1;
		for (uint32 i = 0; i < localFreeList_count; ++i) {
			globalFreeList[startIndex + i] = localFreeList[i];
		}
		globalFreeList[startIndex + localFreeList_count] = id;
		localFreeList_count = 0;
		freeListMutex.unlock();
	}
	// this just increments a counter, it does not actually allocate an id
	void allocate_as() {
		counter++;
	}
	void deallocate_as() {
		counter--;
	}
	void allocate_as_and_setResource(uint32& id, T val) {
		allocate_as();
		setResource(id, val);
	}
#if WEBGPU_USE_REF_SYSTEM
	std::atomic<uint32> res_usage[MAX_IDS] = { 0 };
#endif
	FORCEINLINE T getResource(T input, bool log = true) {
		uint64_t val = getResource((uint64_t)input, log);
		return (T)val;
	}
	FORCEINLINE uint64_t getResource(uint64_t input, bool log = true) {
		if (input >= MAX_IDS) {
			DEBUG_RESOURCE_ERROR(1, DEBUG_TYPE::GET_FAILED, input);
			return 0;
		}
		uint64_t val = res_array[input].load();
		if (val == ALLOC_UNSET) {
			// wait 1ms to see if it gets set
			auto wait_result = res_array[input].wait(ALLOC_UNSET, 100ms);
			val = res_array[input].load();
		}
		else if (val == 0) {
			auto wait_result = res_array[input].wait(0, 100ms);
			val = res_array[input].load();
		}
		if (log) {
			DEBUG_RESOURCE_VERBOSE2(DEBUG_TYPE::GET, input, val);
			DEBUG_RESOURCE_ERROR(val == 0, DEBUG_TYPE::GET_EMPTY, input);
			DEBUG_RESOURCE_ERROR(val == ALLOC_UNSET, DEBUG_TYPE::GET_UNALLOCATED, input);
		}
		return val;
	}
	uint64_t allocateResource() {
		INCREASE_COUNTER(T);
		uint32 out = 0;
		if (!allocate(out)) {
			checkNoEntry();
			//wonder_log::error("allocateResource failed");
			return 0;
		}
		DEBUG_RESOURCE_ERROR(out >= MAX_IDS, DEBUG_TYPE::ALLOCATE_INVALID, out);
		DEBUG_RESOURCE_ERROR(res_array[out].load() != 0, DEBUG_TYPE::ALLOCATE_NONEMPTY, out);
		DEBUG_RESOURCE_VERBOSE(DEBUG_TYPE::ALLOCATE, out);
		res_array[out].store((uint64_t)ALLOC_UNSET); // special value meaning it's allocated but not set
		//res_array[out].notify();
		return (uint64_t)out;
	}
	FORCEINLINE void setResource(uint32 input, T val) {
		check((uint64_t)val != 0);
		setResource(input, (uint64_t)val);
	}
	FORCEINLINE void setResource(uint32 input, uint64_t val) {
		DEBUG_RESOURCE_VERBOSE2(DEBUG_TYPE::SET, input, (uint64_t)val);
		DEBUG_RESOURCE_ERROR(input >= MAX_IDS, DEBUG_TYPE::SET_INVALID, input);
#if DEBUG_RESOURCE
		auto _val = res_array[input].load();
		// wndr-todo: this does not apply to remote client
		DEBUG_RESOURCE_ERROR(_val == 0, DEBUG_TYPE::SET_NOT_ALLOCATED, input);
		DEBUG_RESOURCE_ERROR(_val != ALLOC_UNSET, DEBUG_TYPE::SET_ALREADY_ALLOCATED, input);
#endif
		res_array[input].store(val);
		res_array[input].notify();
	}
	FORCEINLINE void freeResource(uint64_t input) {
		DEBUG_RESOURCE_VERBOSE(DEBUG_TYPE::FREE, input);
		DEBUG_RESOURCE_ERROR(res_array[input].load() == 0, DEBUG_TYPE::FREE, input);
		DEBUG_RESOURCE_ERROR(input == 3, DEBUG_TYPE::FREE, input);
		res_array[input].store(0);
		//res_array[input].notify();
	}
	FORCEINLINE void ref(T input) {
#if WEBGPU_USE_REF_SYSTEM
		DEBUG_RESOURCE_VERBOSE2(DEBUG_TYPE::REF, (uint64_t)input, res_usage[(uint64_t)input] + 1);
		res_usage[(uint64_t)input]++;
#endif
	}
	FORCEINLINE void unref(T input) {
		freeResource((uint64_t)input);
		deallocate((uint64_t)input);
		DECREASE_COUNTER(T);
	}
};
struct WndrProxyBase {
	std::atomic<uint8_t> waitId{ 0 };
	Atomic<uint32> waitIds[256];
	std::string waitData;
	void* waitDataBuffer = 0;
	std::atomic<bool> thread_exit = false;
#if !defined(__EMSCRIPTEN__)
	std::thread our_proxy_thread;
#else
	pthread_t our_proxy_thread;
#endif

	uint32 frame_count = 0;
	bool flushAfterEveryPull = false;
	WndrProxyBase(const WndrProxyBase&) = delete;
	WndrProxyBase(WndrProxyBase&&) = delete;
	WndrProxyBase& operator=(const WndrProxyBase&) = delete;
	WndrProxyBase& operator=(WndrProxyBase&&) = delete;

	virtual FORCEINLINE std::string_view GetIDNameSlow(uint16 ud) {
		return "";
	}

	void unwaitId(uint8_t id) {
		waitIds[id] = false;
		waitIds[id].notify();
	}
	uint8_t getWaitId() {
		auto id = ++waitId;
		waitIds[id] = true;
		return id;
	}
	void waitOnId(uint8_t id) {
		FWndrProbeScope WndrProbe(2); // covers ALL callers, not just push_and_wait — see wndr_probe.h
		if (waitIds[id].wait(true, 30s) == WaitResult::TimedOut) {
			// This used to fall into a SILENT forever-wait (log commented out) — the exact
			// shape of the field freeze. Say it loudly; the heartbeat names the site.
			fprintf(stderr, "[WebGPUProxyWatchdog] waitOnId stuck >30s for id %u — waiting on the wgpu proxy consumer\n", (unsigned)id);
			waitIds[id].wait(true);
		}
	}
	void getWaitData(uint8_t id, std::string& data) {
		data = waitData;
	}
	void getWaitData(uint8_t id, void*& data) {
		data = (void*)waitDataBuffer;
	}
	template<typename T>
	void getWaitData(uint8_t id, T& data) {
#if 0 // wndr-todo
		Archive a(waitDatas[id].data(), waitDatas[id].size());
		a >> data;
#endif
	}
	std::string& getString(uint8_t id) {
		return waitData;
	}
	void* getStringData(uint8_t id) {
		return waitData.data();
	}
	void setWaitData(uint8_t id, int32_t data) {
		waitDataBuffer = (void*)(uint64_t)data;
	}
	template<typename T>
	void setWaitData(uint8_t id, T* data) {
		waitDataBuffer = data;
	}
	WndrProxyBase() = default;

	virtual ~WndrProxyBase() {
		thread_exit = true;
#if defined(__EMSCRIPTEN__)
		pthread_join(our_proxy_thread, nullptr);
#else
		if (our_proxy_thread.joinable()) {
			our_proxy_thread.join();
		}
#endif
	}

#if !DISABLE_ALL_PROFILING
	struct QueueProfiler {
#if ENABLE_VERBOSE_PROFILING
		double times[255] = { 0 };
		int counts[255] = { 0 };
		std::vector<std::tuple<double, int, std::size_t>> pairs = std::vector<std::tuple<double, int, std::size_t>>(255);
		std::vector<std::tuple<double, int, std::size_t>> pairs2 = std::vector<std::tuple<double, int, std::size_t>>(255);
		int profile_count = 0;
#endif
		uint64_t events_since_last = 0;
		double last_time = wndr_get_now();
		void addTime(uint16 id, double time_taken);
	} profiler;
#endif
	//static FORCEINLINE_DEBUGGABLE void init_send(ID id);
};
template <typename Derived>
struct SingletonBase {
	static Derived& get() {
		static Derived instance;
		return instance;
	}

	// Delete copy and move constructors and assign operators
	SingletonBase(const SingletonBase&) = delete; // Copy constructor
	SingletonBase(SingletonBase&&) = delete; // Move constructor
	SingletonBase& operator=(const SingletonBase&) = delete; // Copy assignment
	SingletonBase& operator=(SingletonBase&&) = delete; // Move assignment

protected:
	SingletonBase() = default; // Constructor is protected to prevent instantiation
	~SingletonBase() = default; // Destructor
};
//using RSEQ = atomic_queue::AtomicQueue2<ReshapeWindowElement, 0x10000, true, true, true, false>;
using MethodPtr = void*;
struct WndrProxyMethod : public WndrProxyBase {
	static constexpr WndrProxyType type = WndrProxyType::Method;
	using FunctionPtrType = void(*)(MethodPtr, MethodPtr, struct WndrProxyBase& base);
	struct ArrayQueue {
		static constexpr uint64_t size = 0x1000000;  // Ensure this is a power of two
		static constexpr uint64_t size_mask = size - 1;
		struct alignas(64) Task {
			FunctionPtrType funcPtr = 0;
            Atomic<MethodPtr> arg = (MethodPtr)-1;
            MethodPtr ret = (MethodPtr)-1;
			uint32 waitId = (uint32)-2;
            uint16 id = 0;

			bool vars_set{ false };
			Task() = default;
			Task(FunctionPtrType _funcPtr, MethodPtr _arg, MethodPtr _ret, uint32 _waitId, uint16 _id) : funcPtr(_funcPtr), arg(_arg), ret(_ret), waitId(_waitId), id(_id) {}
			inline void set_args(FunctionPtrType _funcPtr, MethodPtr _arg, MethodPtr _ret, uint32 _waitId, uint16 _id) {
				arg = _arg;
				waitId = _waitId;
				ret = _ret;
				id = _id;
				funcPtr = _funcPtr;
				vars_set = true;
			}
			inline void reset() {
				vars_set = false;
				funcPtr = 0;
				arg = (MethodPtr)-1;
				ret = (MethodPtr)-1;
				waitId = -2;
			}
			inline bool is_set() const {
				return waitId != -2 && ret != (MethodPtr)-1 && arg.load() != (MethodPtr)-1 && vars_set;
				//return vars_set;
			}
		};
		alignas(64) Atomic<uint64_t> p_index{ 0 };
		alignas(64) uint64_t c_index { 0 };
		alignas(64) std::array<Task, size> q;
		ArrayQueue() {
			for (uint64_t i = 0; i < size; i++) {
				q[i].reset();
			}
		}

		inline uint64_t push(FunctionPtrType f, MethodPtr data, MethodPtr ret, uint16 id) {
			return push_and_wait(-1, f, data, ret, id);
		}
		inline uint64_t push_and_wait(uint32 _waitId, FunctionPtrType f, MethodPtr _data, MethodPtr _ret, uint16 _id) {
			uint64_t index = p_index.fetch_add(1);
			q[index].set_args(f, _data, _ret, _waitId, _id);
			return index;
		}
		//bool execute_all();
		uint64_t reset() {
			return p_index.exchange(0);
		}
	};
	virtual bool execute_all() = 0;
	HeapMethodQueue<METHOD_ELEMENT> q{0x100000};
	uint32 thread_id = 0;
	Atomic<uint32_t> idleWakeEpoch{0};
	bool processedWorkLastPull = false;
	bool afterGetCurrentTexture = false;
	uint32_t protectedSpinCount = 0; // WNDR freeze fix: bound the current-texture-window busy-spin

	uint32_t GetIdleWaitUs() const;

	void NotifyIdleWaiter() {
#if defined(__EMSCRIPTEN__)
		idleWakeEpoch.fetch_add(1);
		idleWakeEpoch.notify(1);
#endif
	}

	template<typename TaskType>
	void Enqueue(TaskType&& Task) {
		q.push(std::forward<TaskType>(Task));
		NotifyIdleWaiter();
	}

	bool WaitForIdleWork(uint32_t ObservedEpoch) {
#if defined(__EMSCRIPTEN__)
		const uint32_t IdleWaitUs = GetIdleWaitUs();
		GWndrWgpuProxyIdleWaitUs.store(IdleWaitUs, std::memory_order_relaxed);
		if (IdleWaitUs == 0) {
			return false;
		}

		GWndrWgpuProxyIdleWaits.fetch_add(1, std::memory_order_relaxed);
		const WaitResult Result = idleWakeEpoch.wait(
			ObservedEpoch,
			std::chrono::microseconds(IdleWaitUs));
		if (Result == WaitResult::TimedOut) {
			GWndrWgpuProxyIdleTimeouts.fetch_add(1, std::memory_order_relaxed);
		}
		else {
			GWndrWgpuProxyIdleSignals.fetch_add(1, std::memory_order_relaxed);
		}
		return true;
#else
		return false;
#endif
	}

	void our_method_proxy_loop() {
#if defined(__EMSCRIPTEN__)
		start:
		const uint32_t ObservedEpoch = idleWakeEpoch.load();
#endif
		bool should_flush = execute_all();
#if defined(__EMSCRIPTEN__)
		if (should_flush) {
			//EM_ASM(scheduler.postTask(() => {_our_method_proxy_loop_bridge(Module._this); }, { priority: "user-blocking" }););
			EM_ASM(Module.channel.port2.postMessage(""));
			return;
		}
		if (!processedWorkLastPull && WaitForIdleWork(ObservedEpoch)) {
			// Do not return to the JS event loop while the surface's current texture is held.
			// A JS callback can retire that texture before its queued submit executes.
			if (afterGetCurrentTexture) {
				// WNDR game-tick-freeze fix (2026-08-20): an UNBOUNDED goto-start here LIVELOCKS. While
				// the current-texture window is held we never return to the worker JS event loop, but the
				// async WebGPU callback that would complete the queued submit and CLEAR this window
				// (GetCurrentTexture resolve / OnSubmittedWorkDone / MapAsync) only runs when we DO return.
				// As the scene streams in the window widens until that callback is starved -> present
				// wedges -> render fence never signals -> game thread parks (~frame 40). Bound the spin:
				// after a budget of no-progress iterations, yield anyway so the stuck callback can fire.
				// Normal short windows clear well before the budget, so this only trips on a real stall.
				if (++protectedSpinCount < 128u) {
					goto start;
				}
				protectedSpinCount = 0;
				{ static uint32_t _py = 0; if (_py++ < 200) { printf("WNDR PROXY-PROTECTED-YIELD #%u (spin budget hit in held-texture window -> yield to JS)\n", _py); fflush(stdout); } }
				EM_ASM(Module.channel.port2.postMessage(""));
				return;
			}
			// Outside the protected current-texture window, yield so WebGPU promises and callbacks run.
			protectedSpinCount = 0;
			EM_ASM(Module.channel.port2.postMessage(""));
			return;
		}
		protectedSpinCount = 0;
		if (flushAfterEveryPull) {
			EM_ASM(Module.channel.port2.postMessage(""));
		}
		else {
			goto start;
		}
#endif
	}
	void* ThreadMain() {
		thread_id = FPlatformTLS::GetCurrentThreadId();
#if defined(__EMSCRIPTEN__)
		EM_ASM({
			Module._this = WNDR_JS_POINTER($0);
			Module.channel = new MessageChannel();
			Module.channel.port1.onmessage = () => {
				_our_method_proxy_loop_bridge(Module._this);
			};
			Module.channel.port2.postMessage("");
			}, this);
		emscripten_exit_with_live_runtime();
#else
		while (!thread_exit) {
			our_method_proxy_loop();
		}
#endif
		return nullptr;
	}
	static void* ThreadMainStatic(void* args) {
		return ((WndrProxyMethod*)args)->ThreadMain();
	}
	bool start_our_method_proxy_thread(bool transferCanvas) {
#if defined(__EMSCRIPTEN__)
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		if (transferCanvas) {
			//wonder_log::verbose("Transfering canvas to proxy thread");
			emscripten_pthread_attr_settransferredcanvases(&attr, (const char*)-1);
		}
		pthread_create(&our_proxy_thread, &attr, &WndrProxyMethod::ThreadMainStatic, this);
#else
		our_proxy_thread = std::thread(&WndrProxyMethod::ThreadMainStatic, this);
#endif
		return true;
	};
	void Init(bool transferCanvas, bool _flushAfterEveryPull = false) {
		flushAfterEveryPull = _flushAfterEveryPull;
		start_our_method_proxy_thread(transferCanvas);
	}

	uint32 WaitForCompletion(FunctionPtrType func, MethodPtr args, MethodPtr ret, uint16 id);

	template<typename ID>
	uint64_t push(FunctionPtrType func, MethodPtr args, MethodPtr ret, ID id) {
#if WEBGPU_ALLOW_WAIT_ON_ALL
		static TAutoConsoleVariable<int32> CVarWebGPUAllowWaitOnAll(
			TEXT("r.WebGPU.AllowWaitOnAll"),
			1,
			TEXT("Debugging utility to wait on all operations in the queue.\n")
			TEXT(" 0 - Do not wait on all operations (default)\n")
			TEXT(" 1 - Wait on all operations, useful for debugging and profiling.\n"),
			ECVF_Default
		);
		bool bShouldWait = CVarWebGPUAllowWaitOnAll.GetValueOnAnyThread() == 1;
		if (bShouldWait) {
			auto _waitId = getWaitId();
			uint32 out = 0;
			WndrMarkWgpuProxyQueued((uint16)id);
			Enqueue(ArrayQueue::Task(func, args, ret, (uint32)_waitId, (uint16)id));
			if (thread_id != FPlatformTLS::GetCurrentThreadId()) {
				waitOnId(_waitId);
			}
			return out;
		}
		else {
			WndrMarkWgpuProxyQueued((uint16)id);
			Enqueue(ArrayQueue::Task(func, args, ret, (uint32)-1, (uint16)id));
			return 0;
		}
#else
		WndrMarkWgpuProxyQueued((uint16)id);
		Enqueue(ArrayQueue::Task(func, args, ret, (uint32)-1, (uint16)id));
		return 0;
#endif
	}
	template<typename ID>
	uint64_t push_and_wait(FunctionPtrType func, MethodPtr args, MethodPtr ret, ID id) {
		auto _waitId = getWaitId();
		WndrMarkWgpuProxyQueued((uint16)id);
		Enqueue(ArrayQueue::Task(func, args, ret, (uint32)_waitId, (uint16)id));
		uint64_t out = 0;
		{
			FWndrProbeScope WndrProbe(2); // untimed wgpu-proxy wait — see wndr_probe.h
			waitOnId(_waitId);
		}
		return out;
	}

	template <WndrProxyMode PM, auto func, uint16 func_id, string_litteral func_name, typename... Args>
	auto Send(Args&&... args) {
		using TupleWithoutRefs = std::tuple<std::remove_reference_t<Args>...>;
		if constexpr (PM == WndrProxyMode::Sync) {
			auto copies = std::make_tuple(std::forward<Args>(args)...); // we dont need deep copy because of sync
			push_and_wait([](MethodPtr args, MethodPtr ret, WndrProxyBase& base) {
				//AGGRESSIVE_STATS_SCOPE(func_name.value, FColor::Green);
				auto tuples = (TupleWithoutRefs*)args;
				std::apply([&](auto&&... args) { return
					func(ProcessArg(std::forward<decltype(args)>(args))...);
					}, *tuples);
				}, &copies, (void*)0, (uint16)func_id);
		}
		else {
			auto mem = (TupleWithoutRefs*)copyAllToMemory(std::forward<Args>(args)...);
			push([](MethodPtr args, MethodPtr ret, WndrProxyBase& base) {
				//AGGRESSIVE_STATS_SCOPE(func_name.value, FColor::Cyan);
				auto tuples = (TupleWithoutRefs*)args;
				std::apply([&](auto&&... args) {
					return func(ProcessArg(std::forward<decltype(args)>(args))...);
					}, *tuples);
				}, mem, (void*)0, (uint16)func_id);
		}
	}
	template <WndrProxyMode PM, auto func, uint16 func_id, string_litteral func_name, typename... Args>
	auto SendRetTemp(Args&&... args) {
		using TupleWithoutRefs = std::tuple<std::remove_reference_t<Args>...>;
		using ReturnType = func_return_t<decltype(func)>;
		if constexpr (PM == WndrProxyMode::Sync) {
			ReturnType r{};
			auto copies = std::make_tuple(std::forward<Args>(args)...); // we dont need deep copy because of sync
			push_and_wait([](MethodPtr args, MethodPtr ret, WndrProxyBase& base) {
				//AGGRESSIVE_STATS_SCOPE(func_name.value, FColor::Blue);
				auto r = (ReturnType*)ret;
				auto tuples = (TupleWithoutRefs*)args;
				std::apply([&](auto&&... args) { return
					*r = func(ProcessArg(std::forward<decltype(args)>(args))...);
					}, *tuples);
				}, &copies, &r, (uint16)func_id);
			return r;
		}
		else {
			auto r = IDPool<ReturnType>::get().allocateResource();
			auto mem = (TupleWithoutRefs*)copyAllToMemory(std::forward<Args>(args)...);
			push([](MethodPtr args, MethodPtr ret, WndrProxyBase& base) {
				//AGGRESSIVE_STATS_SCOPE(func_name.value, FColor::Magenta);
				auto r = (uint64_t)ret;
				auto tuples = (TupleWithoutRefs*)args;
				IDPool<ReturnType>::get().setResource(r, std::apply([&](auto&&... args) {
					return func(ProcessArg(std::forward<decltype(args)>(args))...);
					}, *tuples));
				}, mem, (void*)r, (uint16)func_id);
			return (ReturnType)(r);
		}
	}
	virtual ~WndrProxyMethod() {
		thread_exit = true;
	}
};
#if WNDR_INCLUDE_PROXY_API_WEBGPU_METHOD
template<WndrProxyAPI API>
struct WndrMethodProxy : public WndrProxyMethod, public SingletonBase<WndrMethodProxy<API>> {
};
#endif

template<WndrProxyAPI API>
struct WndrNoneProxy : public SingletonBase<WndrNoneProxy<API>> {
};

KEEP_IN_MODULE void unwaitId(WndrProxyBase* q, uint8_t id);

#define POP_WEBGPU
#include "webgpu_override.h"

#if UE_EDITOR
#pragma optimize("", on)
#endif
