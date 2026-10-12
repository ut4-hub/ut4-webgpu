#pragma once
#if PLATFORM_WASM
#include <emscripten/threading.h>

constexpr bool is_diviseble_by(const std::size_t number, const std::size_t divisor) {
	return (number % divisor) == 0;
}

constexpr std::size_t max(const std::size_t a, const std::size_t b) {
	return (a < b) ? b : a;
}

template <typename T, std::size_t PadToMultipleOf, std::size_t AlignmentSize, bool NeedsPadding>
struct alignas(max(AlignmentSize, alignof(T))) AlignedPaddedObjectImpl : T {
	AlignedPaddedObjectImpl() = default;

	template <typename Arg, typename... Args>
	explicit AlignedPaddedObjectImpl(Arg&& arg, Args&&... args)
	    : T{std::forward<Arg>(arg),
	        std::forward<Args>(args)...} {
	}
};

template <typename T, std::size_t PadToMultipleOf, std::size_t AlignmentSize>
struct alignas(max(AlignmentSize, alignof(T))) AlignedPaddedObjectImpl<T, PadToMultipleOf, AlignmentSize, true> : T {
	std::uint8_t padding[PadToMultipleOf - (sizeof(T) % PadToMultipleOf)];

	AlignedPaddedObjectImpl() = default;

	template <typename Arg, typename... Args>
	explicit AlignedPaddedObjectImpl(Arg&& arg, Args&&... args)
	    : T{std::forward<Arg>(arg),
	        std::forward<Args>(args)...} {
	}
};

template <typename T, std::size_t PadToMultipleOf, std::size_t AlignmentSize>
using AlignedPaddedObject = AlignedPaddedObjectImpl<T, PadToMultipleOf, AlignmentSize, !is_diviseble_by(sizeof(T), PadToMultipleOf)>;

enum : std::size_t {
	DESTRUCTIVE_INTERFERENCE_SIZE = 64,
};

template <typename T>
using CacheAlignedAndPaddedObject = AlignedPaddedObject<T,
                                                        DESTRUCTIVE_INTERFERENCE_SIZE,
                                                        DESTRUCTIVE_INTERFERENCE_SIZE>;

namespace ElementState {
enum : uint8_t  // Use enum instead of inline constexpr variable to support C++11
{
	IN_PROGRESS,  // Optional
	READY_FOR_PUSH,
	READY_FOR_POP,
};
}  // namespace ElementState

template <typename T>
struct Element {
	using ElementType = T;
	uint8_t state{ElementState::READY_FOR_PUSH};
	T* value_ptr;
	typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;

	Element() {
		emscripten_atomic_store_u8(&state, ElementState::READY_FOR_PUSH);
	};

	Element(const Element&) = delete;
	Element(Element&&) = delete;

	Element& operator=(const Element&) = delete;
	Element& operator=(Element&&) = delete;

	~Element() {
		if (emscripten_atomic_load_u8(&state) == ElementState::READY_FOR_POP)
			value_ptr->~T();
	}
};

template <template <typename, std::size_t> class Producer,
          template <typename, std::size_t>
          class Consumer,
          typename ElementType,
          std::size_t Count>
struct RingBufferTypeConstructor : Producer<ElementType, Count>, Consumer<ElementType, Count> {
	enum : std::size_t {
		COUNT_MASK = Count - 1,
	};
	static_assert(Count && !(COUNT_MASK & Count), "Count should be a power of two.");

	std::array<CacheAlignedAndPaddedObject<Element<ElementType>>, Count> elements{};

	template <typename... Args>
	bool push(Args&&... args) {
		return this->push_impl(*this, std::forward<Args>(args)...);
	}

	bool try_pop(ElementType& value) {
		return this->pop_impl(*this, value);
	}
};

template <template <typename, std::size_t> class Producer,
          template <typename, std::size_t>
          class Consumer,
          typename ElementType,
          std::size_t Count>
class RingBuffer : RingBufferTypeConstructor<Producer, Consumer, ElementType, Count> {
	using Parrent = RingBufferTypeConstructor<Producer, Consumer, ElementType, Count>;

   public:
	using Parrent::push;
	using Parrent::try_pop;
};

template <typename T>
class wndr_atomic {
public:
	// Constructors
#if PLATFORM_WASM
	constexpr wndr_atomic() noexcept {
		if constexpr (std::is_same_v<T, int8_t>) {
			emscripten_atomic_store_u8(&value_, 0);
		}
		else if constexpr (std::is_same_v<T, int16_t>) {
			emscripten_atomic_store_u16(&value_, 0);
		}
		else if constexpr (std::is_same_v<T, int32_t>) {
			emscripten_atomic_store_u64(&value_, 0);
		}
		else if constexpr (std::is_same_v<T, int64_t>) {
			emscripten_atomic_store_u64(&value_, 0);
		}
	}
	constexpr wndr_atomic(T desired) noexcept {
		static_assert(std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>,
			"Unsupported type for wndr_atomic");
		if constexpr (std::is_same_v<T, int8_t>) {
			emscripten_atomic_store_u8(&value_, static_cast<uint8_t>(desired));
		}
		else if constexpr (std::is_same_v<T, int16_t>) {
			emscripten_atomic_store_u16(&value_, static_cast<uint16_t>(desired));
		}
		else if constexpr (std::is_same_v<T, int32_t>) {
			emscripten_atomic_store_u32(&value_, static_cast<uint32_t>(desired));
		}
		else if constexpr (std::is_same_v<T, int64_t>) {
			emscripten_atomic_store_u64(&value_, static_cast<uint64_t>(desired));
		}
	}

	bool compare_exchange_strong(T& expected, T desired,
		std::memory_order success,
		std::memory_order failure) noexcept {
		return __c11_atomic_compare_exchange_strong(value_, &expected, desired, success, failure);
	}
#else
	constexpr wndr_atomic() noexcept {
		if constexpr (std::is_same_v<T, int8_t>) {
		}
		else if constexpr (std::is_same_v<T, int16_t>) {
		}
		else if constexpr (std::is_same_v<T, int32_t>) {
		}
		else if constexpr (std::is_same_v<T, int64_t>) {
		}
	}
	constexpr wndr_atomic(T desired) noexcept {
		static_assert(std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>,
			"Unsupported type for wndr_atomic");
		if constexpr (std::is_same_v<T, int8_t>) {
		}
		else if constexpr (std::is_same_v<T, int16_t>) {
		}
		else if constexpr (std::is_same_v<T, int32_t>) {
		}
		else if constexpr (std::is_same_v<T, int64_t>) {
		}
	}

	bool compare_exchange_strong(T& expected, T desired,
		std::memory_order success,
		std::memory_order failure) noexcept {
		return __c11_atomic_compare_exchange_strong(value_, &expected, desired, success, failure);
	}
#endif

	wndr_atomic(const wndr_atomic&) = delete;
	wndr_atomic& operator=(const wndr_atomic&) = delete;
	wndr_atomic& operator=(const wndr_atomic&) volatile = delete;

	//void store(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
	//	value_.store(desired, order);
	//}

	//T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
	//	return value_.load(order);
	//}

	// Exchange operations
	//T exchange(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
	//	return value_.exchange(desired, order);
	//}

	alignas(sizeof(T)) T value_;
};

// multi producer start
template <typename ElementType, std::size_t Count>
class MultiProducer {
	CacheAlignedAndPaddedObject<std::atomic_size_t> end{std::size_t(0)};
	CacheAlignedAndPaddedObject<wndr_atomic<int64_t>> push_task_count{static_cast<int64_t>(Count)};
	static_assert(Count <= static_cast<std::size_t>(std::numeric_limits<int64_t>::max()),
	              "Count exceeds the maximum. Count should fit in a std::int64_t.");

   public:
	template <typename Ring>
	void notify_pop(const Ring&) {
		emscripten_atomic_add_u64(&push_task_count.value_, 1);
	}

	template <typename Ring, typename... Args>
	bool push_impl(Ring& ring, Args&&... args) {
		if (emscripten_atomic_sub_u64(&push_task_count.value_, 1) <= int64_t(0)) {
			emscripten_atomic_add_u64(&push_task_count.value_, 1);
			return false;
		}

		while (true) {
			const std::size_t element_index = end.fetch_add(1, std::memory_order_relaxed) & Ring::COUNT_MASK;
			auto& element = ring.elements[element_index];

			uint8_t expected_element_state = ElementState::READY_FOR_PUSH;
			uint8_t desired = uint8_t(ElementState::IN_PROGRESS);
			if (__c11_atomic_compare_exchange_strong((_Atomic uint8_t*)&element.state, &expected_element_state, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
				element.value_ptr = new (&element.storage) ElementType(std::forward<Args>(args)...);

				emscripten_atomic_store_u8(&element.state, ElementState::READY_FOR_POP);
				ring.notify_push(ring);

				return true;
			}
		}
	}
};
// single cosnumer start
template <typename ElementType, std::size_t Count>
class SingleConsumer {
	struct State {
		std::size_t begin{0};
	};

	CacheAlignedAndPaddedObject<State> state;

   public:
	template <typename Ring>
	void notify_push(const Ring&) const {
	}

	template <typename Ring>
	bool pop_impl(Ring& ring, ElementType& value) {
		auto& element = ring.elements[state.begin];

		if (emscripten_atomic_load_u8(&element.state) == ElementState::READY_FOR_POP) {
			value = std::move(*element.value_ptr);
			element.value_ptr->~ElementType();

			emscripten_atomic_store_u8(&element.state, ElementState::READY_FOR_PUSH);
			ring.notify_pop(ring);

			state.begin = (state.begin + 1) & Ring::COUNT_MASK;
			return true;
		} else
			return false;
	}
};

template <typename ElementType, std::size_t Count>
class MultiConsumer {
	CacheAlignedAndPaddedObject<std::atomic_size_t> begin{std::size_t(0)};
	CacheAlignedAndPaddedObject<std::atomic<std::int64_t>> pop_task_count{std::int64_t{0}};

   public:
	template <typename Ring>
	void notify_push(Ring&) {
		pop_task_count.fetch_add(1, std::memory_order_release);
	}

	template <typename Ring>
	bool pop_impl(Ring& ring, ElementType& value) {
		if (pop_task_count.fetch_sub(1, std::memory_order_acq_rel) <= std::int64_t(0)) {
			pop_task_count.fetch_add(1, std::memory_order_relaxed);
			return false;
		}

		while (true) {
			const std::size_t element_index = begin.fetch_add(1, std::memory_order_relaxed) & Ring::COUNT_MASK;
			auto& element = ring.elements[element_index];

			uint8_t expected_element_state = ElementState::READY_FOR_POP;
			uint8_t desired = uint8_t(ElementState::IN_PROGRESS);
			if (__c11_atomic_compare_exchange_strong((_Atomic uint8_t*)&element.state, &expected_element_state, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
				value = std::move(*element.value_ptr);
				element.value_ptr->~ElementType();

				emscripten_atomic_store_u8(&element.state, ElementState::READY_FOR_PUSH);
				ring.notify_pop(ring);
				return true;
			}
		}
	}
};
#else
constexpr bool is_diviseble_by(const std::size_t number, const std::size_t divisor) {
	return (number % divisor) == 0;
}

constexpr std::size_t max(const std::size_t a, const std::size_t b) {
	return (a < b) ? b : a;
}

template <typename T, std::size_t PadToMultipleOf, std::size_t AlignmentSize, bool NeedsPadding>
struct alignas(max(AlignmentSize, alignof(T))) AlignedPaddedObjectImpl : T {
	AlignedPaddedObjectImpl() = default;

	template <typename Arg, typename... Args>
	explicit AlignedPaddedObjectImpl(Arg&& arg, Args&&... args)
		: T{ std::forward<Arg>(arg),
			std::forward<Args>(args)... } {
	}
};

template <typename T, std::size_t PadToMultipleOf, std::size_t AlignmentSize>
struct alignas(max(AlignmentSize, alignof(T))) AlignedPaddedObjectImpl<T, PadToMultipleOf, AlignmentSize, true> : T {
	std::uint8_t padding[PadToMultipleOf - (sizeof(T) % PadToMultipleOf)];

	AlignedPaddedObjectImpl() = default;

	template <typename Arg, typename... Args>
	explicit AlignedPaddedObjectImpl(Arg&& arg, Args&&... args)
		: T{ std::forward<Arg>(arg),
			std::forward<Args>(args)... } {
	}
};

template <typename T, std::size_t PadToMultipleOf, std::size_t AlignmentSize>
using AlignedPaddedObject = AlignedPaddedObjectImpl<T, PadToMultipleOf, AlignmentSize, !is_diviseble_by(sizeof(T), PadToMultipleOf)>;

enum : std::size_t {
	DESTRUCTIVE_INTERFERENCE_SIZE = 64,
};

template <typename T>
using CacheAlignedAndPaddedObject = AlignedPaddedObject<T,
	DESTRUCTIVE_INTERFERENCE_SIZE,
	DESTRUCTIVE_INTERFERENCE_SIZE>;

namespace ElementState {
	enum : std::uint_fast8_t  // Use enum instead of inline constexpr variable to support C++11
	{
		IN_PROGRESS,  // Optional
		READY_FOR_PUSH,
		READY_FOR_POP,
	};
}  // namespace ElementState

template <typename T>
struct Element {
	using ElementType = T;
	std::atomic<std::uint_fast8_t> state{ ElementState::READY_FOR_PUSH };
	T* value_ptr;
	typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;

	Element() = default;

	Element(const Element&) = delete;
	Element(Element&&) = delete;

	Element& operator=(const Element&) = delete;
	Element& operator=(Element&&) = delete;

	~Element() {
		if (state.load(std::memory_order_acquire) == ElementState::READY_FOR_POP)
			value_ptr->~T();
	}
};

template <template <typename, std::size_t> class Producer,
	template <typename, std::size_t>
class Consumer,
	typename ElementType,
	std::size_t Count>
struct RingBufferTypeConstructor : Producer<ElementType, Count>, Consumer<ElementType, Count> {
	enum : std::size_t {
		COUNT_MASK = Count - 1,
	};
	static_assert(Count && !(COUNT_MASK& Count), "Count should be a power of two.");

	std::array<CacheAlignedAndPaddedObject<Element<ElementType>>, Count> elements{};

	template <typename... Args>
	bool push(Args&&... args) {
		return this->push_impl(*this, std::forward<Args>(args)...);
	}

	bool try_pop(ElementType& value) {
		return this->pop_impl(*this, value);
	}
};

template <template <typename, std::size_t> class Producer,
	template <typename, std::size_t>
class Consumer,
	typename ElementType,
	std::size_t Count>
class RingBuffer : RingBufferTypeConstructor<Producer, Consumer, ElementType, Count> {
	using Parrent = RingBufferTypeConstructor<Producer, Consumer, ElementType, Count>;

public:
	using Parrent::push;
	using Parrent::try_pop;
};

// multi producer start
template <typename ElementType, std::size_t Count>
class MultiProducer {
	CacheAlignedAndPaddedObject<std::atomic_size_t> end{ std::size_t(0) };
	CacheAlignedAndPaddedObject<std::atomic<std::int64_t>> push_task_count{ static_cast<std::int64_t>(Count) };
	static_assert(Count <= static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()),
		"Count exceeds the maximum. Count should fit in a std::int64_t.");

public:
	template <typename Ring>
	void notify_pop(const Ring&) {
		push_task_count.fetch_add(1, std::memory_order_release);
	}

	template <typename Ring, typename... Args>
	bool push_impl(Ring& ring, Args&&... args) {
		if (push_task_count.fetch_sub(1, std::memory_order_acq_rel) <= std::int64_t(0)) {
			push_task_count.fetch_add(1, std::memory_order_relaxed);
			return false;
		}

		while (true) {
			const std::size_t element_index = end.fetch_add(1, std::memory_order_relaxed) & Ring::COUNT_MASK;
			auto& element = ring.elements[element_index];

			std::uint_fast8_t expected_element_state = ElementState::READY_FOR_PUSH;
			if (std::atomic_compare_exchange_strong(&element.state, &expected_element_state, std::uint_fast8_t(ElementState::IN_PROGRESS))) {
				element.value_ptr = new (&element.storage) ElementType(std::forward<Args>(args)...);

				element.state.store(ElementState::READY_FOR_POP, std::memory_order_release);
				ring.notify_push(ring);

				return true;
			}
		}
	}
};
// single cosnumer start
template <typename ElementType, std::size_t Count>
class SingleConsumer {
	struct State {
		std::size_t begin{ 0 };
	};

	CacheAlignedAndPaddedObject<State> state;

public:
	template <typename Ring>
	void notify_push(const Ring&) const {
	}

	template <typename Ring>
	bool pop_impl(Ring& ring, ElementType& value) {
		auto& element = ring.elements[state.begin];

		if (element.state.load(std::memory_order_acquire) == ElementState::READY_FOR_POP) {
			value = std::move(*element.value_ptr);
			element.value_ptr->~ElementType();

			element.state.store(ElementState::READY_FOR_PUSH, std::memory_order_release);
			ring.notify_pop(ring);

			state.begin = (state.begin + 1) & Ring::COUNT_MASK;
			return true;
		}
		else
			return false;
	}
};

template <typename ElementType, std::size_t Count>
class MultiConsumer {
	CacheAlignedAndPaddedObject<std::atomic_size_t> begin{ std::size_t(0) };
	CacheAlignedAndPaddedObject<std::atomic<std::int64_t>> pop_task_count{ std::int64_t{0} };

public:
	template <typename Ring>
	void notify_push(Ring&) {
		pop_task_count.fetch_add(1, std::memory_order_release);
	}

	template <typename Ring>
	bool pop_impl(Ring& ring, ElementType& value) {
		if (pop_task_count.fetch_sub(1, std::memory_order_acq_rel) <= std::int64_t(0)) {
			pop_task_count.fetch_add(1, std::memory_order_relaxed);
			return false;
		}

		while (true) {
			const std::size_t element_index = begin.fetch_add(1, std::memory_order_relaxed) & Ring::COUNT_MASK;
			auto& element = ring.elements[element_index];

			std::uint_fast8_t expected_element_state = ElementState::READY_FOR_POP;
			if (std::atomic_compare_exchange_strong(&element.state, &expected_element_state, std::uint_fast8_t(ElementState::IN_PROGRESS))) {
				value = std::move(*element.value_ptr);
				element.value_ptr->~ElementType();

				element.state.store(ElementState::READY_FOR_PUSH, std::memory_order_release);
				ring.notify_pop(ring);
				return true;
			}
		}
	}
};

#endif

static constexpr std::size_t RingSize = 65536;
static constexpr std::size_t NumberOfTries = 1024;
using FetcherRingBuffer = RingBuffer<MultiProducer,
                                     SingleConsumer,
                                     datahash,
                                     RingSize>;
using DecompressorRingBuffer = RingBuffer<MultiProducer,
                                          MultiConsumer,
                                          datahash,
                                          RingSize>;
using LoggerRingBuffer = RingBuffer<MultiProducer,
                                    SingleConsumer,
                                    std::string,
                                    RingSize>;  // could be 131072 plus enable higher compression, if using dedicated thread