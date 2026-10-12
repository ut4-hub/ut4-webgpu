#pragma once
//#include "tuplet.h"
#include "wasm_atomic.h"
#include "AtomicQueue.h"
#include "wonder_log.h"
#include <vector>
#include <array>
#include <span>
#include <string_view>
#define DEEP_COPY_THREAD_SAFE 1
#define DEEPCOPY_USE_LARGE_BUFFER 1
#define USE_ATOMIC_DEEP_COPY 1 // perf hog
#define WRAP_AROUND 1
#define USE_PFR 0
#define CHECKED 0 // perf hog
#define ARRAY_SIZE_TYPE uint32_t
#if USE_PFR
#include "pfr.hpp"
#endif

#if UE_EDITOR
#pragma optimize("", off)
#endif

template <typename T, typename = void>
struct is_complete : std::false_type {};
template <typename T, typename = void>
struct is_complete_and_aggregate : std::false_type {};

template <typename T>
struct is_complete_and_aggregate<T, std::void_t<decltype(sizeof(T))>>
    : std::integral_constant<bool, std::is_aggregate_v<T>> {};
template <typename T, typename = void>
struct is_complete_and_pointer : std::false_type {};
template <typename T>
struct is_complete_and_pointer<T*, std::void_t<decltype(sizeof(T))>> : std::true_type {};
using u32v = std::array<uint32_t, 8>;
struct DeepCopyString {
    const char* str;
    DeepCopyString(const char* s) : str(s) {}
    operator const char* () const { return str; }
};
template<typename T>
struct is_array_holder : std::false_type {};
template<typename T>
struct is_array_holder<ArrayHolder<T>> : std::true_type {};
template<typename T>
struct is_array_holder<T*> : is_array_holder<T> {};
template<typename T>
struct is_array_holder<T&> : is_array_holder<T> {};
template<typename T>
struct is_array_holder<const T&> : is_array_holder<T> {};
struct DeepCopyUtils {
    char* buffer;
    uint32_t pos;
    DeepCopyUtils(char* buffer, uint32_t pos) : buffer(buffer), pos(pos) {}
    inline void* alloc(uint32_t size) {
        auto out = (char*)buffer + pos;
        pos += size;
        return out;
    }

    template<typename T>
    inline void serialize2(T& field) {
        if constexpr (has_serialize<T>::value) {
            // should not get here
            //serialize(field);
			//wndr_checkNoEntry();
			abort();
		}
        else {
            void* allocated_memory = alloc(sizeof(T));

            // Cast allocated memory to T*
            using NonConstT = std::remove_const_t<T>;
            NonConstT* out = static_cast<NonConstT*>(allocated_memory);
            *out = field;
            // make non cosnt
            //*out = field;
        }
    }
    template<typename T>
    void serialize_array(const uint32_t count, const T* const& arr) {
        serialize(count);
        for (int i = 0; i < count; i++) {
            serialize(arr[i]);
        }
    }
    template<typename T>
    void deserialize_array(size_t& count, T const*& arr) {
        uint32_t _count;
        deserialize(_count);
        arr = (T*)alloc_usage(sizeof(T) * _count);
        for (int i = 0; i < _count; i++) {
            deserialize((T&)arr[i]);
        }
        count = _count;
    }

    template<typename T>
    void serialize_impl(const T& in_field) {
        if constexpr (is_complete_and_pointer<T>::value) {
            if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, char>) {
                serialize_string(in_field);
            }
            else {
                bool* has_value = (bool*)alloc(1);
                *has_value = in_field != nullptr;
                if (in_field != nullptr) {
                    serialize(*in_field);
                }
                else {
                    //serialize2(in_field);
                }
            }
        }
        else if constexpr (std::is_aggregate_v<T> || std::is_class_v<T>) {
#if USE_PFR
            pfr::for_each_field(in_field, [this](const auto& field) {
                serialize(field);
                });
#else
            static_assert(has_serialize<T>::value, "No serialize method found");
#endif
        }
        else if constexpr (std::is_enum_v<T>) {
			serializeToBuffer(in_field);
		}
        else {
            serialize2(in_field);
        }
    }

    template<typename T>
    inline typename std::enable_if<!is_array_holder<T>::value, void>::type
    serialize(const T& t) {
        using NonRefNonConstT = std::remove_const_t<std::remove_reference_t<T>>;
        if constexpr (std::is_arithmetic_v<NonRefNonConstT>) {
            serializeToBuffer(t);
        }
        else {
            serialize_impl(t);
        }
    }
    void serialize_string(const char* field, size_t size) {
        uint32_t* _size = (uint32_t*)alloc(4);
        *_size = (uint32_t)size;
        const char* out = (const char*)alloc((uint32_t)size);
        //strcpy_s((char*)out, size, field);
        std::copy(field, field + size, (char*)out);
    }
    void serialize_string(const char* field) {
        if (field == nullptr) {
			serialize((uint32_t)0);
            return;
		}
        uint32_t size = (uint32_t)strlen(field) + 1;
        serialize_string(field, size);
    }

    // Overload for pointer types, forwards to the primary template as a non-pointer type
    //template<typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
    //inline void serialize(const T t) {
    //    serialize(*t); // Dereference and forward to non-pointer serialize
    //}

    template<typename T>
    void remove_const_and_deserialize(T& field) {
        if constexpr (std::is_pointer_v<T>) {
            using NonConstPointerType = std::remove_const_t<std::remove_pointer_t<T>>*;
            NonConstPointerType modifiable_ptr = const_cast<NonConstPointerType>(field);
            deserialize_packed(modifiable_ptr);
        }
        else {
            T& modifiable_value = const_cast<std::remove_const_t<T>&>(field);
            deserialize_packed(modifiable_value);
        }
    }

    template<typename T>
    inline void deserialize_simple(T& out) {
        memcpy((void*)&out, buffer + pos, sizeof(T));
        pos += sizeof(T);
    }
    template<typename T>
    void deserialize_impl(T& in_field) {
		if constexpr (std::is_void_v<T> || (std::is_pointer_v<T> && std::is_void_v<std::remove_pointer_t<T>>)) {
			static_assert(!std::is_void_v<T>, "Cannot deserialize void type");
			// Handle void pointers appropriately here (maybe do nothing or provide a specialized implementation)
			return;
		}
        else if constexpr (is_complete_and_pointer<T>::value) {
            bool has_value = *(bool*)(buffer + pos);
            pos += 1;
            if (has_value) {
                in_field = (T)alloc_usage(sizeof(*in_field));
                //in_field = (T)malloc(sizeof(*in_field));
                // zero out?
                if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, char>) {
                    //in_field = (T)alloc(sizeof(T));
                    deserialize(in_field);
                }
                else {
                    // remove const
                    using NonConstPointerType = std::remove_const_t<std::remove_pointer_t<T>>*;
                    deserialize(*const_cast<NonConstPointerType>(in_field));
                }
            }
            else {
                in_field = nullptr;
            }
        }
        else if constexpr (std::is_aggregate_v<T> || std::is_class_v<T>) {
#if USE_PFR
            pfr::for_each_field(in_field, [this](auto& field) {
                using FieldType = std::decay_t<decltype(field)>;
                constexpr bool is_pointer = std::is_pointer_v<FieldType>;
                constexpr bool is_char_ptr = std::is_same_v<FieldType, const char*>;
                constexpr bool is_WGPUChainedStruct_ptr = std::is_same_v<FieldType, WGPUChainedStruct>;
                if constexpr (is_pointer) {
                    // get the bool value
                    uint32_t has_value = *(uint32_t*)(buffer + pos);
                    if constexpr (!is_char_ptr) {
                        //pos += 4;
                    }
                    if (has_value > 0) {
                        // get the value
                        deserialize(field);
                    }
                    else {
                        FieldType* nonConstFieldPtr = const_cast<FieldType*>(&field);
                        *nonConstFieldPtr = nullptr;
                    }
                }
                else {
                    deserialize(field);
                }
                });
#else
            static_assert(has_deserialize<T>::value, "No Deserialize method found");
#endif
        }
        else if constexpr (std::is_enum_v<T> || std::is_arithmetic_v<T>) {
			deserialize_simple(in_field);
		}
        else {
            deserialize(in_field);
        }
    }
    template<typename T>
    inline typename std::enable_if<!is_array_holder<T>::value, void>::type
    deserialize(T& t) {
        deserialize_impl(t);
    }
    template<typename T>
    inline typename std::enable_if<!is_array_holder<T>::value, void>::type
    deserialize(T*& t) {
        deserialize_impl(t);
    }

    template<>
    void deserialize(char*& field) {
        uint32_t size = *(uint32_t*)(buffer + pos);
        pos += 4;
        field = (char*)alloc(size);
    }
    template<>
    void deserialize(const char*& field) {
        uint32_t size = *(uint32_t*)(buffer + pos);
        pos += 4;
        if (size > 0)
            field = (const char*)alloc(size);
        else
            field = nullptr;
    }

    void deserialize_string(const char*& field, uint32_t& size) {
        size = *(uint32_t*)(buffer + pos);
        pos += 4;
        field = (const char*)alloc(size);
    }

    template<typename T>
    inline void serializeToBuffer(const T& value) {
        memcpy(buffer + pos, &value, sizeof(T));
        pos += sizeof(T);
    }

    template<typename T>
    inline void _serialize2(const T& t) {
        if constexpr (has_serialize<T>::value) {
            // Specialized serialize method exists
            serialize(t);
        }
        else {
            // Fallback to a generic method
            serializeToBuffer(t);
        }
    }
    template<typename T>
    static inline uint32_t size_impl(const T& in_field) {
        uint32_t _size = 0;
        if constexpr (is_complete_and_pointer<T>::value) {
            if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, char>) {
                _size += 4; // + sizeof(uint32_t) for the size of the string
                _size += strlen(in_field) + 1;
            }
            else {
                if (in_field != nullptr) {
                    _size += size(*in_field);
                }
            }
        }
        else if constexpr (std::is_aggregate_v<T> || std::is_class_v<T>) {
#if USE_PFR
            pfr::for_each_field(in_field, [&_size](const auto& field) {
                _size += size(field);
                });
#else
            static_assert(has_size<T>::value, "Serialization using PFR is disabled and no specialization is provided to size.");
            return 0; // Placeholder to satisfy return type requirement
#endif
        }
        else if constexpr (std::is_void_v<T> || std::is_function_v<T>) {
			_size += 0;
		}
        else {
            _size += sizeof(T);
        }
        return _size;
    }
    template<typename T>
    static inline typename std::enable_if<!is_array_holder<T>::value, uint32_t>::type
    size(const T& t) {
        return size_impl(t);
    }
    template<typename T>
    static inline typename std::enable_if<!is_array_holder<T>::value, uint32_t>::type
    size(const T* t) {
        // sizeof(uint32_t) for the exists flag
        if (t != nullptr) {
            return size(*t) + sizeof(bool);
        }
        return sizeof(bool);
    }
    template <typename... Args>
    static inline uint32_t all_sizes(const Args&... args) {
        uint32_t size_to_alloc = 0;
        ((size_to_alloc += (size(args))), ...);
        return size_to_alloc;
    }
    template <std::size_t N = 8, typename... Args, std::size_t... Is>
    static inline u32v all_sizes_impl(u32v& arr, std::index_sequence<Is...>, const Args&... args) {
        //uint32_t index = Is + 1;
        ((arr[Is + 1] = all_sizes(args)), ...);
        return arr;
    }

    template <typename... Args>
    static inline u32v all_sizes(u32v& positions, const Args&... args) {
        positions[0] = 0; // Assign the first element to 0
        return all_sizes_impl(positions, std::index_sequence_for<Args...>{}, args...);
    }

    // direct

    template<typename T>
    void deserialize_direct(T*& field, uint32_t offset) {
        field = (T*)(buffer + offset);
    }

    void deserialize_direct(const char*& field, uint32_t offset) {
        auto out = (DeepCopyString*)(buffer + offset);
        field = *out;
    }

    template<typename T>
    void deserialize_direct(T& field, uint32_t offset) {
        auto out = (T*)(buffer + offset);
        field = *out;
    }

    template<typename T>
    T* allocate_and_copy(const T& source) {
        T* dest = (T*)(buffer + pos);
        pos += sizeof(T);
        *dest = source;
        return dest;
    }

    const char* allocate_and_copy(const char* source) {
        size_t len = strlen(source) + 1;
        char* dest = buffer + pos;
        pos += (uint32_t)len;
        //strcpy_s(dest, len, source);
        std::copy(source, source + len, dest);
        return dest;
    }

    template<typename T>
    T* serialize_direct_impl(const T& source) {
#if USE_PFR
        T* dest = (T*)allocate_and_copy(source);

        pfr::for_each_field(source, [this, dest](const auto& field, const auto index) {
            using FieldType = std::decay_t<decltype(field)>;
            constexpr bool is_pointer = std::is_pointer_v<FieldType>;
            constexpr bool is_char_ptr = std::is_same_v<FieldType, const char*>;

            if constexpr (is_pointer) {
                if constexpr (is_char_ptr) {
                    // Special handling for const char* (C-strings)
                    auto copied_field = allocate_and_copy(field);
                    *reinterpret_cast<const char**>(&pfr::get<index>(*dest)) = copied_field;
                }
                else {
                    // Handling for pointers to other structures
                    using PointedType = std::remove_pointer_t<std::remove_const_t<FieldType>>;
                    constexpr bool IsComplete = is_complete<PointedType>::value;
                    if (field != nullptr) {
                        //if constexpr (IsComplete) {
                        auto copied_field = serialize_direct<PointedType>(*field);
                        // remove const because we get error C2440: 'reinterpret_cast': cannot convert from 'const Arg *' to 'PointedType **'

                        *(PointedType**)(
                            &pfr::get<index>(*dest)
                            ) =
                            copied_field;
                        //}
                        //else {
                            // If the pointed type is not complete, we can't copy it
                            // So we just copy the pointer itself
                            //memcpy(buffer + pos, &field, sizeof(FieldType*));
                            //pos += sizeof(FieldType*);
                            //*reinterpret_cast<FieldType**>(&pfr::get<index>(*dest)) = field;
                        //}
                    }
                }
            }
            });

        return dest;
#else
        static_assert(has_serialize_direct<T>::value, "Serialization using PFR is disabled and no specialization is provided.");
        return nullptr; // Placeholder to satisfy return type requirement
#endif
    }
#if 0
    const char* serialize_direct_impl(const char*& source) {
        const char* dest = (const char*)allocate_and_copy(source);
        auto copied_field = allocate_and_copy(source);
        *reinterpret_cast<const char**>(&dest) = copied_field;
        return dest;
    }
#endif
    template<typename T>
    static size_t size_direct_impl(const T& source) {
#if USE_PFR
        size_t total_size = 0;
        total_size = sizeof(T);
        pfr::for_each_field(source, [&total_size](const auto& field) {
            using FieldType = std::decay_t<decltype(field)>;
            constexpr bool is_pointer = std::is_pointer_v<FieldType>;
            constexpr bool is_char_ptr = std::is_same_v<FieldType, const char*>;

            if constexpr (is_pointer) {
                if constexpr (is_char_ptr) {
                    // Add size of the string (including null terminator)
                    if (field != nullptr) {
                        total_size += strlen(field) + 1;
                    }
                }
                else {
                    // For nested structures, recursively calculate the size
                    using PointedType = std::remove_pointer_t<std::remove_const_t<FieldType>>;
                    constexpr bool IsCompleteOrIsSimpleType = is_complete<PointedType>::value || std::is_arithmetic_v<PointedType>;
                    if (field != nullptr) {
                        //if constexpr (IsCompleteOrIsSimpleType) {
                        total_size += size_direct<PointedType>(*field);
                        //}
                        //else {
                            // just get the size of the pointer itself
                        //    total_size += sizeof(FieldType*);
                        //}
                    }
                }
            }
            });
        return total_size;
#else
        static_assert(has_size_direct<T>::value, "Serialization using PFR is disabled and no specialization is provided.");
        return 0; // Placeholder to satisfy return type requirement
#endif
    }
    template<typename T>
    inline T* serialize_direct(const T& source) {
        return serialize_direct_impl(source);
    }
    template<typename T>
    inline T* serialize_direct(const T* source) {
        return (T*)serialize_direct_impl(source);
    }

    template<typename T>
    static inline size_t size_direct(const T& source) {
        return size_direct_impl(source);
    }

    static inline size_t size_direct(const char*& source) {
        size_t total_size = 0;
        total_size = sizeof(const char*);
        total_size += strlen(source) + 1;
        return total_size;
    }
#if 0
    static inline size_t size_direct(WGPUBufferDescriptor* const& source) {
        size_t total_size = 0;
        total_size += sizeof(WGPUBufferDescriptor);
        total_size += size_direct(source->label);
        return total_size;
    }
#endif
    template <std::size_t N = 8, typename... Args, std::size_t... Is>
    static inline u32v all_sizes_impl_direct(u32v& arr, std::index_sequence<Is...>, const Args&... args) {
        ((arr[Is + 1] = size_direct(args)), ...);
        return arr;
    }

    template <typename... Args>
    static inline u32v all_sizes_direct(u32v& positions, const Args&... args) {
        positions[0] = 0; // Assign the first element to 0
        return all_sizes_impl_direct(positions, std::index_sequence_for<Args...>{}, args...);
    }

    template<typename T>
    uint32_t _serialize_direct(T&& source) {
        serialize_direct(std::forward<T>(source));
        return pos;
    }

    struct serialize_tag {};
    struct deserialize_tag {};
    struct serialize_direct_tag {};
    struct size_tag {};
    template<typename T>
    inline void process_field(serialize_tag, T&& field, uint32_t& _size) {
        serialize(std::forward<T>(field));
    }

    template<typename T>
    inline void process_field(deserialize_tag, T&& field, uint32_t& _size) {
        deserialize(std::forward<T>(field));
    }

    template<typename T>
    inline void process_field(serialize_direct_tag, T&& field, uint32_t& _size) {
        serialize_direct(std::forward<T>(field));
    }

    template<typename T>
    static inline void process_field(size_tag, T&& field, uint32_t& _size) {
        _size += size(std::forward<T>(field));
    }

    template<typename T>
    inline void process_array(serialize_tag, T&& field, size_t count, uint32_t& _size) {
        serialize_array(count, std::forward<T>(field));
    }

    template<typename T>
    inline void process_array(deserialize_tag, T&& field, size_t& count, uint32_t& _size) {
        deserialize_array(count, std::forward<T>(field));
    }

    template<typename T>
    inline void process_array(serialize_direct_tag, T&& field, size_t count, uint32_t& _size) {
    }

    template<typename T>
    static inline void process_array(size_tag, T&& field, size_t& count, uint32_t& _size) {
    }
    void serialize_every_secondPairs() {}

    template<typename First, typename Second, typename... Rest>
    void serialize_every_secondPairs(First&&, Second&& second, Rest&&... rest) {
        serialize(std::forward<Second>(second)); // Directly call the function with the second argument.

        if constexpr (sizeof...(Rest) > 0) {
            serialize_every_secondPairs(std::forward<Rest>(rest)...); // Recursive call with the remaining arguments.
        }
    }

    template<typename... Args>
    void serialize_every_second(Args&&... args) {
        serialize_every_secondPairs(std::forward<Args>(args)...);
    }

    void deserialize_every_secondPairs() {}

    template<typename First, typename Second, typename... Rest>
    void deserialize_every_secondPairs(First&&, Second&& second, Rest&&... rest) {
        if constexpr (is_array_holder<Second>::value) {
			deserialize(std::forward<Second>(second));
		}
        else {
            deserialize(std::forward<Second>(second)); // Directly call the function with the second argument.
        }
    }

    template<typename... Args>
    void deserialize_every_second(Args&&... args) {
        deserialize_every_secondPairs(std::forward<Args>(args)...);
    }

    static uint32_t size_every_secondPairs() { return 0; }

    template<typename First, typename Second, typename... Rest>
    static uint32_t size_every_secondPairs(First&&, Second&& second, Rest&&... rest) {
        uint32_t out = size(std::forward<Second>(second));
        if constexpr (sizeof...(Rest) > 0) {
            out += size_every_secondPairs(std::forward<Rest>(rest)...);
        }
        return out;
    }

    template<typename... Args> static uint32_t size_every_second(Args&&... args) {
        return size_every_secondPairs(std::forward<Args>(args)...);
    }
    template<typename T>
    inline typename std::enable_if<is_array_holder<T>::value, void>::type
    serialize(const T& rsource) {
        if constexpr (std::is_pointer_v<T>) {
            if (rsource == nullptr) {
				serialize((ARRAY_SIZE_TYPE)0);
				return;
			}
            serialize((ARRAY_SIZE_TYPE)rsource->length);
            for (int i = 0; i < rsource->length; i++) {
                serialize(rsource->ptr[i]);
            }
		}
		else {
			serialize(rsource.length);
            for (int i = 0; i < rsource.length; i++) {
				serialize(rsource.ptr[i]);
			}
		}
    }
    //template<typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
    //inline void serialize(const T t) {
    //    serialize(*t); // Dereference and forward to non-pointer serialize
    //}
    template<typename T>
    inline typename std::enable_if<is_array_holder<typename std::decay<T>::type>::value, void>::type
    deserialize(T& rsource) {
        deserialize(rsource.length);
		rsource.ptr = (T*)alloc_usage(sizeof(T) * rsource.length);
    }
#if 0
    template<typename T>
    inline typename std::enable_if<is_array_holder<typename std::decay<T>::type>::value, void>::type
    deserialize(T*& t) {
        deserialize(t->length);
        t->ptr = (T*)alloc_usage(sizeof(T) * t->length);
    }
#endif
    template<typename T>
    inline typename std::enable_if<is_array_holder<typename std::decay<T>::type>::value, void>::type
    deserialize(T* t) {
        // arrays have already been allocated, so we just need to fill them
        size_t* out_size = (size_t*)t->length;
        auto out_data = (T**)t->ptr;
        ARRAY_SIZE_TYPE _size;
        deserialize(_size); // limit to 4GB
        t->length = _size;
        if (t->length == 0) {
			*out_size = 0;
			*out_data = nullptr;
			return;
		}
        //deserialize(t->ptr);
        //t->ptr = (decltype(t->ptr))alloc_usage((uint32_t)sizeof(T) * t->length);
        size_t s = (size_t)sizeof(decltype(t->ptr[0])) * t->length;
        t->ptr = (decltype(t->ptr))alloc_usage(s);
        //ZeroMemory(t->ptr, s);
        for (int i = 0; i < t->length; i++) {
			deserialize(t->ptr[i]);
		}
        *out_size = t->length;
        *out_data = (T*)t->ptr;
        //wonder_log::error("Deserializing array holder with pointer");
    }
    template<typename T>
    static inline typename std::enable_if<is_array_holder<typename std::decay<T>::type>::value, uint32_t>::type
    size(const T& rsource) {
        uint32_t _size = sizeof(ARRAY_SIZE_TYPE);
        if constexpr (std::is_pointer_v<T>) {
            if (rsource == nullptr || rsource->length == 0) {
                return _size;
            }
            for (int i = 0; i < rsource->length; i++) {
				_size += size(rsource->ptr[i]);
			}
        }
        else {
            for (int i = 0; i < rsource.length; i++) {
                _size += size(rsource.ptr[i]);
            }
        }
        return _size;
    }
    template <typename T, typename = void>
    struct has_serialize : std::false_type {};
    template <typename T>
    struct has_serialize<T, std::void_t<decltype(&DeepCopyUtils::serialize<const T&>)>> : std::true_type {};
    template <typename T, typename = void>
    struct has_serialize_direct : std::false_type {};
    template <typename T>
    struct has_serialize_direct<T, std::void_t<decltype(&DeepCopyUtils::serialize_direct<const T&>)>> : std::true_type {};
    template <typename T, typename = void>
    struct has_deserialize : std::false_type {};
    template <typename T>
    struct has_deserialize<T, std::void_t<decltype(&DeepCopyUtils::deserialize<const T&>)>> : std::true_type {};
    template <typename T, typename = void>
    struct has_size : std::false_type {};
    template <typename T>
    struct has_size<T, std::void_t<decltype(&DeepCopyUtils::size<const T&>)>> : std::true_type {};
    template <typename T, typename = void>
    struct has_size_direct : std::false_type {};
    template <typename T>
    struct has_size_direct<T, std::void_t<decltype(&DeepCopyUtils::size_direct<const T&>)>> : std::true_type {};
    template <typename T, typename = void>
    struct has_deserialize_array_method : std::false_type {};
    template <typename T>
    struct has_deserialize_array_method<T, std::void_t<decltype(&DeepCopyUtils::deserialize_array<T>)>> : std::true_type {};

    template<>
    void serialize(const std::string& value) {
        auto size = uint32_t(value.size());
        memcpy(buffer + pos, &size, sizeof(uint32_t));
        pos += sizeof(uint32_t);
        memcpy(buffer + pos, value.data(), size);
        pos += size;
    }
    template<>
    uint32_t size(const char* value) {
        if (value == nullptr)
			return sizeof(uint32_t);
        return uint32_t(strlen(value) + 1) + sizeof(uint32_t); // + sizeof(uint32_t) for the size of the string
    }
    template<>
    void deserialize(std::string& value) {
        uint32_t size;
        memcpy(&size, buffer + pos, sizeof(uint32_t));
        pos += sizeof(uint32_t);
        //assign_addr(value);
        pos += size;
    }
    template<>
    inline uint32_t size(const std::string& value) {
        return uint32_t(sizeof(std::string) + (uint32_t)value.size() + 1) + sizeof(uint32_t); // + sizeof(uint32_t) for the size of the string
    }
};

struct DeepCopy {
    std::span<char> span;
    char* buffer;
    uint32_t size;
    uint32_t alloc_size;
#if DEEP_COPY_THREAD_SAFE
    Atomic<uint32_t> pos;
    Atomic<uint32_t> locked;
    //Atomic<uint32_t> range_start;
    inline uint32_t atomic_fetch_add_with_wraparound(uint32_t addend) {
        uint32_t expected = pos.load();
        uint32_t desired;
        uint32_t original;
        do {
            original = expected;
            desired = (expected + addend) % (size + 1);
        } while (!pos.compare_exchange_weak(expected, desired));
        return original;
    }
#else
    uint32_t pos;
#endif
    uint64_t total_bytes = { 0 };
    Atomic<uint64_t> bytes_transferred_since_last_send;
    double last_print = wndr_get_now();

    std::string_view GetView() {
        //size_t start = range_start.exchange(pos);
        //auto view = std::string_view(buffer + start, pos - start);
		//return view;
        return std::string_view(buffer, pos);
	}

    DeepCopy(uint32_t _size) {
        size = _size;
        alloc_size = _size;
        buffer = (char*)small_malloc(size);
        span = std::span<char>(buffer, size);
        pos = 0;
    }

    DeepCopy(const char* _data, uint32_t _size) {
		size = _size;
        alloc_size = 0;
        buffer = (char*)_data;
		span = std::span<char>(buffer, size);
		pos = 0;
    }

    DeepCopy(std::string_view _data) {
		size = (uint32_t)_data.size();
        alloc_size = 0;
        buffer = (char*)_data.data();
		span = std::span<char>(buffer, size);
		pos = 0;
    }

    void reset() {
        pos = 0;
        locked = 0;
        // calculate mbps since last print
    }
    void reset(const char* _data, uint32_t _size, bool own) {
        pos = 0;
        size = _size;
        //range_start = 0;
        if (own && _size > alloc_size) {
            alloc_size = _size;
            buffer = (char*)small_realloc(buffer, _size, size);
            memcpy(buffer, _data, _size);
        }
        else {
            alloc_size = 0;
            buffer = (char*)_data;
        }
        span = std::span<char>((char*)buffer, _size);
    }

    template<typename T>
    inline void alignOffset() {
        uint32_t alignment = alignof(T);
        pos = (pos + alignment - 1) & ~(alignment - 1);
    }

    template <typename T>
    inline auto handle_arg(T&& arg) -> decltype(auto) {
        return std::forward<T>(arg);
    }

    // Overload for pointer types: dereferences the pointer to get a reference
    template <typename T>
    inline auto handle_arg(T* arg) -> decltype(auto) {
        return *arg; // Dereference the pointer
    }
    inline auto handle_arg(const char* arg) -> decltype(auto) {
        return DeepCopyString(arg);
    }
    template <typename... Args>
    u32v serialize_direct(Args&&... args) {
        u32v offsets;
        uint32_t old = 0;
        uint32_t size_to_alloc = 0;
        DeepCopyUtils::all_sizes_direct(offsets, args...); // can also just use 100b blocks if memory is not an issue
        for (int i = 1; i < sizeof...(Args) + 1; i++) {
            size_to_alloc += offsets[i];
        }
#if DEEP_COPY_THREAD_SAFE
		old = atomic_fetch_add_with_wraparound(size_to_alloc);
#else
        old = pos;
        pos += size_to_alloc;
#endif
        for (int i = 0; i < sizeof...(Args) + 1; i++) {
            offsets[i] += old;
        }
        DeepCopyUtils utils(buffer, old);
        (utils._serialize_direct(handle_arg(args)), ...);
        return offsets;
    }
    template<typename T>
    void deserialize_direct(T&& field, uint32_t offset) {
        DeepCopyUtils utils(buffer, 0);
        utils.deserialize_direct(std::forward<T>(field), offset);
    }

    template <typename... Args>
    std::string_view serialize(const Args&... args) {
        uint32_t old = 0;
        uint32_t size_to_alloc = 0;
#if DEEP_COPY_THREAD_SAFE
        size_to_alloc = DeepCopyUtils::all_sizes(args...);
		old = pos.fetch_add(size_to_alloc);
		if (old + size_to_alloc > size) {
			pos = old;
			return std::string_view();
		}
        //old = atomic_fetch_add_with_wraparound(size_to_alloc);
#else
        old = pos;
        size_to_alloc = DeepCopyUtils::size(args...);
        pos += size_to_alloc;
#endif

        DeepCopyUtils utils(buffer, old);
        (utils.serialize(args), ...);
        return std::string_view(buffer + old, size_to_alloc);
    }
    template <typename... Args>
    std::string_view serialize_unsafe(const Args&... args) {
        uint32_t old = 0;
        uint32_t size_to_alloc = 0;
        old = pos;
        size_to_alloc = DeepCopyUtils::all_sizes(args...);
        pos += size_to_alloc;

        DeepCopyUtils utils(buffer, old);
        (utils.serialize(args), ...);
        return std::string_view(buffer + old, size_to_alloc);
    }
    void deserialize_recursive(uint32_t& offset, size_t index) {}

    template <typename T, typename... Args>
    void deserialize_recursive(uint32_t& offset, size_t index, T& first, Args&... args) {
        DeepCopyUtils utils(buffer, offset);
        utils.deserialize(first);
        deserialize_recursive(utils.pos, index + 1, args...);
        offset = utils.pos;
    }

    template <typename... Args>
    void deserialize(uint32_t& offset, Args&&... args) {
        deserialize_recursive(offset, 0, args...);
    }

    void deserialize_direct_recursive(const u32v offsets, size_t index) {}

    template <typename T, typename... Args>
    void deserialize_direct_recursive(const u32v offsets, size_t index, T& first, Args&... args) {
        deserialize_direct(first, offsets[index]);
        deserialize_direct_recursive(offsets, index + 1, args...);
    }

    template <typename... Args>
    void deserialize_direct(const u32v offsets, Args&&... args) {
        deserialize_direct_recursive(offsets, 0, args...);
    }
};
#if 0 // example of how to define methods for a type
template<>
void DeepCopyUtils::serialize(const std::string& value) {
    auto size = uint32_t(value.size());
    memcpy(buffer + pos, &size, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    memcpy(buffer + pos, value.data(), size);
    pos += size;
}
template<>
uint32_t DeepCopyUtils::size(const char* value) {
    return uint32_t(strlen(value) + 1) + sizeof(uint32_t); // + sizeof(uint32_t) for the size of the string
}
template<>
void DeepCopyUtils::deserialize(std::string& value) {
    uint32_t size;
    memcpy(&size, buffer + pos, sizeof(uint32_t));
    pos += sizeof(uint32_t);
    //assign_addr(value);
    pos += size;
}
template<>
static inline uint32_t DeepCopyUtils::size(const std::string& value) {
    return uint32_t(sizeof(std::string) + (uint32_t)value.size() + 1) + sizeof(uint32_t); // + sizeof(uint32_t) for the size of the string
}
#endif
#if UE_EDITOR
#pragma optimize("", on)
#endif