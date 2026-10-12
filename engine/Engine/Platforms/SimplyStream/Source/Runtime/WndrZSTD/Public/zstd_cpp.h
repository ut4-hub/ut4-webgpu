#pragma once
//#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
//#include "Modules/ModuleManager.h"
#include <string>
#include <string_view>
#include <vector>

class WNDRZSTD_API FZSTD : public IModuleInterface {
public:
	static std::string ApplyDiff(const std::string& from, const std::string& patch, const uint32_t new_size);
	static std::string GenDiff(const std::string& from, const std::string& to);
	static std::string compress(const std::string& to_compress, int value = 11);
	static std::string compress(std::string_view to_compress, int value = 11);
	static std::string_view decompress(std::string_view to_decompress, bool free_old = true);
	static std::string_view decompress(std::string_view to_decompress, const int out_size, bool free_old = true, uint32_t hash = 0);
};

typedef struct ZSTD_CDict_s ZSTD_CDict;
typedef struct ZSTD_DDict_s ZSTD_DDict;
std::string wndr_compress(std::string_view to_compress, int value);
std::string wndr_compress(const char* to_compress, uint32_t size, int value);
size_t wndr_compress2(const char* to_compress, uint32_t size, const char* compressed, int value);
size_t wndr_compress2_dict(const char* to_compress, uint32_t size, const char* compressed, int value, ZSTD_CDict* const cdict);
std::string wndr_decompress(std::string_view to_decompress, uint32_t dsize, bool free_old = true);
std::string wndr_decompress(const char* to_decompress, uint32_t size, uint32_t dsize, bool free_old = true);
void wndr_decompress(std::string& to_decompress, uint32_t dsize);
size_t wndr_decompress2(const char* to_decompress, uint32_t size, uint32_t dsize, const char* uncompressed, bool free_old);
size_t wndr_decompress2_dict(const char* to_decompress, uint32_t size, uint32_t dsize, const char* uncompressed, bool free_old, ZSTD_DDict* ddict);
extern "C" void zstd_compress(const std::string & to_compress, int value, std::string & out);

// DIFF TOOLS
size_t GenDiff(const char* from_data, const size_t fome_size, const char* to_data, const size_t to_size, const char* compressed, uint32_t level);
size_t ApplyDiff(std::string_view from, std::string_view patch, const size_t new_size, const char* patched);

// Proxy functions
WNDRZSTD_API void wndrProxyReshapeWindow(void* hWnd, int32 NewX, int32 NewY, int32 NewWidth, int32 NewHeight);
WNDRZSTD_API void wndrProxyWindowCreate(void* hWnd, int32 NewX, int32 NewY, int32 NewWidth, int32 NewHeight, uint32_t WindowStyle, uint32_t WindowExStyle, uint64_t parent, const char* title);
WNDRZSTD_API void wndrProxyWindowPos(void* hWnd, int32 X, int32 Y);
WNDRZSTD_API void wndrProxyWindowRegion(void* hWnd, int32 Width, int32 Height);
WNDRZSTD_API void wndrProxyWindowShow(void* hWnd, uint8 cmd);
WNDRZSTD_API void wndrProxyWindowHide(void* hWnd);
WNDRZSTD_API void wndrProxyWindowDestroy(void* hWnd);
WNDRZSTD_API void wndrProxyWindowMinimize(void* hWnd);
WNDRZSTD_API void wndrProxyWindowMaximize(void* hWnd);
WNDRZSTD_API void wndrProxyWindowRestore(void* hWnd);
WNDRZSTD_API void wndrProxyWindowActivate(void* hWnd);
WNDRZSTD_API void wndrProxyWindowFocus(void* hWnd);
WNDRZSTD_API void wndrProxyWindowEnable(void* hWnd, bool cmd);
