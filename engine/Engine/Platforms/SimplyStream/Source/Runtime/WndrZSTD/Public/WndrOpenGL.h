#pragma once
#include "opengl_defines.h"

#define WNDR_INCLUDE_PROXY_API_WEBGL_METHOD		0
#define WNDR_INCLUDE_PROXY_API_WEBGL_NETWORK	0

#if WNDR_INCLUDE_PROXY_API_WEBGL_METHOD
template<>
struct WndrProxy<WndrProxyAPI::WebGL> : public WndrProxyBase {
	static WndrProxy& get() {
		static WndrProxy* instance = nullptr;
		static std::mutex mtx;
		if (instance == nullptr) {
			std::lock_guard<std::mutex> lock(mtx);
			if (instance == nullptr) {
				WndrProxy* temp = new WndrProxy;
				instance = temp;
			}
		}
		return *instance;
	}
	u64 adapter;
	u64 device;
	u64 instance;
	u64 default_queue;
	u64 surface;
	u64 evt_count = 0;
	enum class ID : uint16_t {
		WGPU_ALL(ENUM_COMMA)
		OTHER_FUNCS(ENUM_COMMA)
		COUNT
	};
	static constexpr WNDR_FORCE_INLINE std::string_view GetIDName(const ID id) {
		return wndr_id_names[(int)id];
	}
	static constexpr std::array<const char*, (int)ID::COUNT + 1> wndr_id_names = {
	WGPU_ALL(WGPU_ID_TO_STRING)
	OTHER_FUNCS(WGPU_ID_TO_STRING)
	"COUNT"
	};
#if !DISABLE_ALL_PROFILING
	struct QueueProfiler {
#if ENABLE_VERBOSE_PROFILING
		double times[255] = { 0 };
		int counts[255] = { 0 };
		std::vector<std::tuple<double, int, std::size_t>> pairs = std::vector<std::tuple<double, int, std::size_t>>(255);
		std::vector<std::tuple<double, int, std::size_t>> pairs2 = std::vector<std::tuple<double, int, std::size_t>>(255);
		int profile_count = 0;
#endif
		u64 events_since_last = 0;
		double last_time = wndr_get_now();
		void addTime(ID id, double time_taken);
	} profiler;
#endif
	class WebGLErrorScope {
	public:
		WebGLErrorScope(WGPUDevice device, const ID id)
			: device_(device), id_(id) {
			if (device_ == nullptr) {
				return;
			}
			wgpuDevicePushErrorScope(device_, WGPUErrorFilter_Validation);
		}

		~WebGLErrorScope() {
			if (device_ == nullptr) {
				return;
			}
			// Using a lambda for the callback
			auto callback = [](WGPUErrorType errorType, const char* message, void* userdata) {
				if (WGPUErrorType::WGPUErrorType_NoError == errorType) {
					return;
				}
				wonder_log::error("WGPU_ERROR: {} failed validation with {}", (const char*)userdata, message);
				};
			const auto msg = GetIDName(id_);
			wgpuDevicePopErrorScope(device_, callback, (void*)msg.data());
		}

		WebGLErrorScope(const WebGLErrorScope&) = delete;
		WebGLErrorScope& operator=(const WebGLErrorScope&) = delete;

		WebGLErrorScope(WebGLErrorScope&&) = default;
		WebGLErrorScope& operator=(WebGLErrorScope&&) = default;

	private:
		const ID id_;
	};
};
using wglProxy = WndrProxy<WndrProxyAPI::WebGL>;
#endif

enum class GL_ID : uint16_t {
	GL_FUNCS(COMMA)
	OTHER_FUNCS(COMMA)
	PLATFORM_FUNCS(COMMA)
	COUNT
};

static HashT<GL_ID, std::string> id_names = {
	GL_FUNCS(ITEM)
	OTHER_FUNCS(ITEM)
	PLATFORM_FUNCS(ITEM)
};

enum class Source {
	GLSL,
	HLSL,
};
enum class Semantics {
	OpenGL,
	Vulkan
};

// Enum for compilation target.
enum class Target {
	AST,
	Spv,
	BothASTAndSpv,
};
#if 0
struct ShaderResult {
	std::string shaderName;
	std::string output;
	std::string error;
};
struct GlslangResult {
	std::vector<ShaderResult> shaderResults;
	std::string linkingOutput;
	std::string linkingError;
	bool validationResult;
	std::string spirvWarningsErrors;
	std::string spirv;  // Optional SPIR-V disassembly text.
};
#endif
