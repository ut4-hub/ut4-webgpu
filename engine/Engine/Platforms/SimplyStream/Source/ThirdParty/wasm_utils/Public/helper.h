#pragma once
#define UWS_NO_ZLIB
#define HAS_FLZMA2 0
#define HAS_BROTLI 0
#define HAS_ZSTD 1
#define HAS_BCM 0
#define HAS_LZ4 0
#define USE_WEBSOCKETS 0
#define MAX_STRING_WAIT_TIME 500000000

#include <filesystem>
#include <string>
#include <chrono>
#include <cmath>
#include <sstream>
#include <fstream>
#include "archive.h"
#include <chrono>
#include <cstdlib>
#include <map>
#include <future>
#pragma push_macro("check") // memorize current check macro
#undef check
#pragma pop_macro("check")
#ifdef _WIN32
#define pclose _pclose
#define popen _popen
#define chdir _chdir
#endif

#ifndef KEEP_IN_MODULE
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

std::vector<std::string> split(std::string strToSplit, char delimeter);

uint32_t bytes_to_u32(const std::string& str);
std::string u32_to_bytes(const uint32_t num);

std::string bytesToStr(uint64_t bytes);
bool path_has_prefix(const fs::path& path, const fs::path& prefix);
uint32_t genRandomId();
fnhash GetFnHash(const std::string& fn);