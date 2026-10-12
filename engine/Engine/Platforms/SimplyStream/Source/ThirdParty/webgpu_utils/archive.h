#pragma once
#include <stdint.h>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/atomic.h>
#elif defined(_WIN32)
#include "Windows/AllowWindowsPlatformTypes.h"
#include <intrin.h>
#include "Windows/HideWindowsPlatformTypes.h"
#else 
#endif
#if __has_include(<print>)
#include <format>
#include <print>
#else
#define FMT_HEADER_ONLY
#define FMT_EXCEPTIONS 0
#pragma push_macro("check")  // memorize current check macro
#undef check
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4583)
#pragma warning(disable : 4582)
#include "fmt/format.h"
#include "fmt/xchar.h"  // wchar support
#pragma warning(pop)
#else
#include "fmt/format.h"
#include "fmt/xchar.h"  // wchar support
#endif
#pragma pop_macro("check")  // memorize current check macro
#endif
#ifndef WNDR_BUILD_FOR_NODE
#define WNDR_BUILD_FOR_NODE 0
#endif

#ifndef KEEP_IN_MODULE
#ifdef __EMSCRIPTEN__
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#else
#define KEEP_IN_MODULE extern "C"
#endif
#endif

#if 1
#include "parallel_hashmap/phmap.h"
#include "parallel_hashmap/phmap_dump.h"
using phmap::flat_hash_map;
using phmap::parallel_node_hash_map;
using phmap::parallel_flat_hash_map;
using phmap::parallel_flat_hash_set;
using phmap::parallel_node_hash_set;
using phmap::node_hash_map;
#define EXTRAARGS                                                         \
	, phmap::priv::hash_default_hash<K>, phmap::priv::hash_default_eq<K>, \
	    std::allocator<std::pair<K, V>>, 4, std::mutex
#define EXTRAARGS2                                                        \
	, phmap::priv::hash_default_hash<K>, phmap::priv::hash_default_eq<K>, \
	    std::allocator<K>, 4, std::mutex
template <class K, class V>
using HashT = parallel_flat_hash_map<K, V EXTRAARGS>;
template <class K, class V>
using HashT_Node = parallel_node_hash_map<K, V EXTRAARGS>;
template <class K>
using SetT = parallel_flat_hash_set<K EXTRAARGS2>;
template <class K>
using SetT_Node = parallel_node_hash_set<K EXTRAARGS2>;
template <class K, class V>
using HashT_ST = flat_hash_map<K, V>;
template <class K, class V>
using HashT_ST_Node = node_hash_map<K, V>;
#else
#include <unordered_map>
#include <unordered_set>
template <class K, class V>
using HashT = std::unordered_map<K, V>;
template <class K>
using SetT = std::unordered_set<K>;
#endif

struct AssetInfo {
	std::string data = "";
	std::string uc_data = "";
	uint32_t size = 0, crc = 0;
};
struct DataView {
	uint8_t* data = nullptr;
	uint32_t size = 0;
	DataView() {}
	DataView(const char* str)
		: data((uint8_t*)str), size(sizeof(str) + 1) {}
	DataView(const void* d, const uint32_t s)
		: data((uint8_t*)d), size(s) {}
};
struct DataCopy {
	uint8_t* data = nullptr;
	uint32_t size = 0;
	DataCopy() {}
	DataCopy(const char* str)
		: data((uint8_t*)str), size((uint32_t)strlen(str)) {}
	DataCopy(const void* d, const uint32_t s)
		: data((uint8_t*)d), size(s) {}
	~DataCopy() {}
};
template<typename T>
struct DataCopyForMethod {
	T* data = nullptr;
	uint32_t size = 0;
	bool ownsData = false;
	DataCopyForMethod() {}
	DataCopyForMethod(const DataCopyForMethod& other) {
		data = (T*)malloc(other.size);
		memcpy((void*)data, other.data, other.size);
		size = other.size;
		ownsData = true;
	}
	DataCopyForMethod(T* d, const uint32_t s)
		: data(d), size(s) {}
	~DataCopyForMethod() {
		if (ownsData)
			free((void*)data);
	}
};
struct RefView {
	void* data = nullptr;
};
using fnhash = uint32_t;
using asset_index = uint32_t;
using datahash = uint64_t;
#define MAKE_HASH64(str) XXH64(str.data(), str.size(), 0)
#define MAKE_FNHASH(str) XXH32(str.data(), str.size(), 0)
struct AssetHash {
	constexpr static uint64_t SIZE_BITS = 32; // 2^32 = 4GB max size
	constexpr static uint64_t FLAG_BITS = 64 - SIZE_BITS;
	struct AssetMeta {
		bool disallow_evict = false;
		bool allow_range = false;
		bool black_list = false;
		bool load_on_start = false;
	};
	AssetHash() = default;
	datahash hash = 0;
	uint64_t data = 0;
	std::vector<asset_index> dep_map;
	uint64_t get_size() {
		return data >> FLAG_BITS;
	}
	void set_size(const uint64_t s) {
		data = (s << FLAG_BITS) | (data & ((1ULL << FLAG_BITS) - 1));
	}
	void set_data(AssetMeta meta) {
		data |= (meta.disallow_evict << (FLAG_BITS - 1)) | (meta.allow_range << (FLAG_BITS - 2)) | (meta.black_list << (FLAG_BITS - 3)) | (meta.load_on_start << (FLAG_BITS - 4));
	}
	AssetMeta get_data() {
		AssetMeta meta;
		meta.disallow_evict = (data >> (FLAG_BITS - 1)) & 1;
		meta.allow_range = (data >> (FLAG_BITS - 2)) & 1;
		meta.black_list = (data >> (FLAG_BITS - 3)) & 1;
		meta.load_on_start = (data >> (FLAG_BITS - 4)) & 1;
		return meta;
	}
	bool get_disallow_evict() const {
		return (data >> (FLAG_BITS - 1)) & 1;
	}
	bool get_allow_range() const {
		return (data >> (FLAG_BITS - 2)) & 1;
	}
	bool get_black_list() const {
		return (data >> (FLAG_BITS - 3)) & 1;
	}
	bool get_load_on_start() const {
		return (data >> (FLAG_BITS - 4)) & 1;
	}
	void set_disallow_evict(bool allow_evict) {
		data = (data & ~(1ULL << (FLAG_BITS - 1))) | (allow_evict << (FLAG_BITS - 1));
	}
	void set_allow_range(bool allow_range) {
		data = (data & ~(1ULL << (FLAG_BITS - 2))) | (allow_range << (FLAG_BITS - 2));
	}
	void set_black_list(bool black_list) {
		data = (data & ~(1ULL << (FLAG_BITS - 3))) | (black_list << (FLAG_BITS - 3));
	}
	void set_load_on_start(bool load_on_start) {
		data = (data & ~(1ULL << (FLAG_BITS - 4))) | (load_on_start << (FLAG_BITS - 4));
	}
};
struct GameInfoDebug {
	std::vector<std::string> index_to_fn;
};
struct GameInfo {
	std::vector<AssetHash> index_to_assethash;
	HashT<fnhash, asset_index> fnhash_to_index;
	HashT<fnhash, std::vector<std::pair<bool, std::string>>> directories;
	std::string GameID = "";
};
struct Archive {
	bool owns = false;
	bool corrupt = false;
	uint32_t pos = 0;
	uint32_t size = 0;
	std::mutex mtx;
	std::string data;
	const char* view;

	Archive(const char* str, const uint32_t size)
		: view(str)
	{
		owns = false;
		this->size = size;
	}
	Archive()
	{
		owns = true;
		view = data.data();
	}

	void Lock() {
		mtx.lock();
	}

	void Unlock() {
		mtx.unlock();
	}

	void reset()
	{
		pos = 0;
	}

	void full_reset()
	{
		pos = 0;
		size = 0;
	}

	void SetCorrupt() {
		corrupt = true;
		exit(1);
	}

	void read(char* data2, const uint32_t size2, const bool m_free = false)
	{
		std::memcpy(data2, view + pos, size2);
		pos += size2;
	}

	void write(const char* data2, const uint32_t size2)
	{
		data.append(data2, size2);
		view = data.data();
		pos += size2;
		size += size2;
	}
};

// read/write for arithmetic and pointer types
template <
	typename T,
	typename std::enable_if<std::is_arithmetic<T>::value || std::is_enum<T>::value || std::is_pointer<T>::value, int>::type = 0
>
Archive & operator>>(Archive & ar, T & v)
{
	ar.read(reinterpret_cast<char*>(&v), sizeof(v));
	return ar;
}

template <
	typename T,
	typename std::enable_if<std::is_arithmetic<T>::value || std::is_enum<T>::value, int>::type = 0
>
Archive & operator<<(Archive & ar, const T v)
{
	ar.write(reinterpret_cast<const char*>(&v), sizeof(v));
	return ar;
}

template <typename T, typename = void>
struct has_serialize : std::false_type {};

template <typename T>
struct has_serialize<T, std::void_t<decltype(std::declval<const T&>().serialize(std::declval<Archive&>()))>> : std::true_type {};

template <
	typename T,
	typename std::enable_if<has_serialize<T>::value, int>::type = 0
>
Archive& operator>>(Archive& ar, T& v)
{
	v.serialize(ar);
	return ar;
}

template <
	typename T,
	typename std::enable_if<has_serialize<T>::value, int>::type = 0
>
Archive& operator<<(Archive& ar, const T& v)
{
	v.serialize(ar);
	return ar;
}

#define ARCHIVE_OUT(x) static Archive& operator>>(Archive& ar, x& v)
#define ARCHIVE_IN(x) static Archive& operator<<(Archive& ar, const x& v)
#define ARCHIVE_DECLARE_IN_OUT(x) Archive& operator>>(Archive& ar, x& v); Archive& operator<<(Archive& ar, const x& v)

ARCHIVE_DECLARE_IN_OUT(AssetHash);
ARCHIVE_DECLARE_IN_OUT(GameInfo);
ARCHIVE_DECLARE_IN_OUT(GameInfoDebug);
ARCHIVE_DECLARE_IN_OUT(Archive);

ARCHIVE_OUT(DataCopy) {
	ar >> v.size;
	if (v.size == 0)
		return ar;
	v.data = (uint8_t*)(ar.view + ar.pos);
	ar.pos += v.size;
	return ar;
}
ARCHIVE_IN(DataCopy) {
	ar << v.size;
	if (v.data == nullptr) {
		return ar;
	}
	ar.write((const char*)v.data, v.size);
	return ar;
}

// std::string
ARCHIVE_OUT(std::string) {
	if (ar.corrupt)
		return ar;
	// VerifyType(ARCHIVE_STRING);
	uint32_t len;
	ar >> len;
	if (len > ar.size) {
		ar.SetCorrupt();
		return ar;
	}
	v.resize(len);
	ar.read((char*)v.data(), len);
	return ar;
}
ARCHIVE_IN(std::string) {
	uint32_t size2 = (uint32_t)v.size();
	ar << size2;
	ar.write((const char*)v.data(), size2);
	return ar;
}

// std::u16string
ARCHIVE_OUT(std::u16string) {
	if (ar.corrupt)
		return ar;
	// VerifyType(ARCHIVE_STRING);
	uint32_t len;
	ar >> len;
	if (len > ar.size) {
		ar.SetCorrupt();
		return ar;
	}
	v.resize(len);
	ar.read((char*)v.data(), len);
	return ar;
}
ARCHIVE_IN(std::u16string) {
	uint32_t size2 = (uint32_t)v.size();
	ar << size2;
	ar.write((const char*)v.data(), size2);
	return ar;
}
#if defined(__cpp_lib_string_view) && __cpp_lib_string_view >= 201606L
Archive& operator<<(Archive& ar, const std::string_view v);
#endif

template <class T>
Archive& operator>>(Archive& ar, std::vector<T>& v) {
	if (ar.corrupt)
		return ar;
	uint32_t size2 = 0;
	ar >> size2;
	if (size2 > ar.size) {
		ar.SetCorrupt();
		return ar;
	}
	v.resize(size2);
	for (auto& _v : v)
		ar >> _v;
	return ar;
}
template <class T>
Archive& operator<<(Archive& ar, const std::vector<T>& v) {
	uint32_t size2 = (uint32_t)v.size();
	ar << size2;
	for (auto& _v : v)
		ar << _v;
	return ar;
}
template <class T, class T2>
Archive& operator>>(Archive& ar, HashT<T, T2>& v) {
	if (ar.corrupt)
		return ar;
	uint32_t size2 = 0;
	ar >> size2;
	v.reserve(size2);
	for (uint32_t i = 0; i < size2; ++i) {
		std::pair<T, T2> value;
		ar >> value;
		v.insert(v.end(), value);
	}
	return ar;
}
template <class T, class T2>
Archive& operator<<(Archive& ar, const HashT<T, T2>& v) {
	uint32_t size2 = (uint32_t)v.size();
	ar << size2;
	for (auto& _v : v)
		ar << _v;
	return ar;
}
template <class T>
Archive& operator>>(Archive& ar, SetT<T>& v) {
	if (ar.corrupt)
		return ar;
	uint32_t size2 = 0;
	ar >> size2;
	v.reserve(size2);
	for (uint32_t i = 0; i < size2; ++i) {
		T value;
		ar >> value;
		v.insert(v.end(), value);
	}
	return ar;
}
template <class T>
Archive& operator<<(Archive& ar, const SetT<T>& v) {
	uint32_t size2 = v.size();
	ar << size2;
	for (auto& _v : v)
		ar << _v;
	return ar;
}
template <class T, class T2>
Archive& operator>>(Archive& ar, std::pair<T, T2>& v) {
	ar >> v.first;
	ar >> v.second;
	return ar;
}
template <class T, class T2>
Archive& operator<<(Archive& ar, const std::pair<T, T2>& v) {
	ar << v.first;
	ar << v.second;
	return ar;
}