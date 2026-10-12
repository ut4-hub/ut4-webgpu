#pragma once

#define WGPU_USING_BG_CACHE 1

#define WGPU_PROXY_NONE 0
#define WGPU_PROXY_METHOD 1
#define WGPU_PROXY_REMOTE 2

#define WEBGPU_CONVERT_TO_ASYNC ((PLATFORM_WASM || 1) && 1)
#define RB_CHECKED 0

#if UE_EDITOR
#define WGPU_PROXY_TYPE WGPU_PROXY_METHOD
#else
#define WGPU_PROXY_TYPE WGPU_PROXY_METHOD
#endif

#define MAKE_WGPUStringView(d) {d, d ? strlen(d) : 0}
#define MAKE_WGPUStringView3(d) {d, sizeof(d)}

#if WGPU_PROXY_TYPE == WGPU_PROXY_METHOD
#define WNDR_INCLUDE_PROXY_API_WEBGPU_METHOD	1
#define WNDR_INCLUDE_PROXY_API_WEBGPU_REMOTE	0
#elif WGPU_PROXY_TYPE == WGPU_PROXY_REMOTE
#define WNDR_INCLUDE_PROXY_API_WEBGPU_METHOD	0
#define WNDR_INCLUDE_PROXY_API_WEBGPU_REMOTE	1
#else
#define WNDR_INCLUDE_PROXY_API_WEBGPU_METHOD	0
#define WNDR_INCLUDE_PROXY_API_WEBGPU_REMOTE	0
#endif

#define WGPU_USE_GET_MAPPED_RANGE_AND_WRITE (1 && WGPU_PROXY_TYPE != WGPU_PROXY_REMOTE)
#define NON_BLOCKING_WGPU_BUFFER_GET_MAPPED_RANGE_AND_WRITE 0
#define ENABLE_RENDERPASS_DEBUG PLATFORM_WINDOWS && !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && 1
#define WGPU_ATLAS_BUFFER_BINDING 0
#define WGPU_ATLAS_BIND_GROUP_RENDER 2
#define WGPU_ATLAS_BIND_GROUP_COMPUTE 1

#if ENABLE_RENDERPASS_DEBUG
extern std::atomic<uint64> g_renderpass_count;
extern uint64 gCurrectRenderPassIndex;
struct FRenderPassDebugInfo
{
	std::string name;
	std::string pipeline_name;
};
extern std::string global_current_renderpass;
extern std::vector<FRenderPassDebugInfo> global_last_renderpasses;
extern std::string renderpass_and_pipelines;
#endif

// Include this file instead of webgpu/webgpu.h as it provides webgpu.h along with:
// - utility functions
// - abstractions to paper over the differences between webgpu.h implementations
// - the ability to replace certain webgpu.h functions with custom implementations

#ifndef WGPU_STRIDE_UNDEFINED
#define WGPU_STRIDE_UNDEFINED (0xffffffffUL) // TODO: Remove when Emscripten gets this
#endif

static void syncWebGPU() {
}
#define WGPU_FUNCS(ENUM)            \
		ENUM(wgpuCreateInstance)	\
		ENUM(wgpuGetProcAddress)	\

// Methods of Adapter
#define WGPU_FUNCS_ADAPTER(ENUM)			\
		ENUM(wgpuAdapterCreateDevice)		\
		ENUM(wgpuAdapterEnumerateFeatures)	\
		ENUM(wgpuAdapterGetInstance)		\
		ENUM(wgpuAdapterGetLimits)\
		ENUM(wgpuAdapterGetInfo)\
		ENUM(wgpuAdapterHasFeature)\
		ENUM(wgpuAdapterRequestDevice)\
		ENUM(wgpuAdapterReference)		\
		ENUM(wgpuAdapterRelease)	\

		// Methods of BindGroup
#define WGPU_FUNCS_BINDGROUP(ENUM)  \
		ENUM(wgpuBindGroupSetLabel)	\
		ENUM(wgpuBindGroupReference)\
		ENUM(wgpuBindGroupRelease)	\

		// Methods of BindGroupLayout
#define WGPU_FUNCS_BINDGROUPLAYOUT(ENUM)  \
		ENUM(wgpuBindGroupLayoutSetLabel)\
		ENUM(wgpuBindGroupLayoutReference)\
		ENUM(wgpuBindGroupLayoutRelease)\

		// Methods of Buffer
#define WGPU_FUNCS_BUFFER(ENUM)  \
		ENUM(wgpuBufferDestroy)\
		ENUM(wgpuBufferGetConstMappedRange)\
		ENUM(wgpuBufferGetMapState)\
		ENUM(wgpuBufferGetMappedRange)\
		ENUM(wgpuBufferGetSize)\
		ENUM(wgpuBufferGetUsage)\
		ENUM(wgpuBufferMapAsync)\
		ENUM(wgpuBufferSetLabel)\
		ENUM(wgpuBufferUnmap)\
		ENUM(wgpuBufferReference)\
		ENUM(wgpuBufferRelease)\

		// Methods of CommandBuffer
#define WGPU_FUNCS_COMMANDBUFFER(ENUM)  \
		ENUM(wgpuCommandBufferSetLabel)\
		ENUM(wgpuCommandBufferReference)\
		ENUM(wgpuCommandBufferRelease)\

		// Methods of CommandEncoder
#define WGPU_FUNCS_COMMANDENCODER(ENUM)  \
		ENUM(wgpuCommandEncoderBeginComputePass)\
		ENUM(wgpuCommandEncoderBeginRenderPass)\
		ENUM(wgpuCommandEncoderClearBuffer)\
		ENUM(wgpuCommandEncoderCopyBufferToBuffer)\
		ENUM(wgpuCommandEncoderCopyBufferToTexture)\
		ENUM(wgpuCommandEncoderCopyTextureToBuffer)\
		ENUM(wgpuCommandEncoderCopyTextureToTexture)\
		ENUM(wgpuCommandEncoderCopyTextureToTextureInternal)\
		ENUM(wgpuCommandEncoderFinish)\
		ENUM(wgpuCommandEncoderInjectValidationError)\
		ENUM(wgpuCommandEncoderInsertDebugMarker)\
		ENUM(wgpuCommandEncoderPopDebugGroup)\
		ENUM(wgpuCommandEncoderPushDebugGroup)\
		ENUM(wgpuCommandEncoderResolveQuerySet)\
		ENUM(wgpuCommandEncoderSetLabel)\
		ENUM(wgpuCommandEncoderWriteBuffer)\
		ENUM(wgpuCommandEncoderWriteTimestamp)\
		ENUM(wgpuCommandEncoderReference)\
		ENUM(wgpuCommandEncoderRelease)\

		// Methods of ComputePassEncoder
#define WGPU_FUNCS_COMPUTEPASSENCODER(ENUM)  \
		ENUM(wgpuComputePassEncoderDispatchWorkgroups)\
		ENUM(wgpuComputePassEncoderDispatchWorkgroupsIndirect)\
		ENUM(wgpuComputePassEncoderEnd)\
		ENUM(wgpuComputePassEncoderInsertDebugMarker)\
		ENUM(wgpuComputePassEncoderPopDebugGroup)\
		ENUM(wgpuComputePassEncoderPushDebugGroup)\
		ENUM(wgpuComputePassEncoderSetBindGroup)\
		ENUM(wgpuComputePassEncoderSetLabel)\
		ENUM(wgpuComputePassEncoderSetPipeline)\
		ENUM(wgpuComputePassEncoderWriteTimestamp)\
		ENUM(wgpuComputePassEncoderReference)\
		ENUM(wgpuComputePassEncoderRelease)\

		// Methods of ComputePipeline
#define WGPU_FUNCS_COMPUTEPIPELINE(ENUM)  \
		ENUM(wgpuComputePipelineGetBindGroupLayout)\
		ENUM(wgpuComputePipelineSetLabel)\
		ENUM(wgpuComputePipelineReference)\
		ENUM(wgpuComputePipelineRelease)\

		// Methods of Device
#define WGPU_FUNCS_DEVICE(ENUM)  \
		ENUM(wgpuDeviceCreateBindGroup)\
		ENUM(wgpuDeviceCreateBindGroupLayout)\
		ENUM(wgpuDeviceCreateBuffer)\
		ENUM(wgpuDeviceCreateCommandEncoder)\
		ENUM(wgpuDeviceCreateComputePipeline)\
		ENUM(wgpuDeviceCreateComputePipelineAsync)\
		ENUM(wgpuDeviceCreateErrorBuffer)\
		ENUM(wgpuDeviceCreateErrorExternalTexture)\
		ENUM(wgpuDeviceCreateErrorTexture)\
		ENUM(wgpuDeviceCreateExternalTexture)\
		ENUM(wgpuDeviceCreatePipelineLayout)\
		ENUM(wgpuDeviceCreateQuerySet)\
		ENUM(wgpuDeviceCreateRenderBundleEncoder)\
		ENUM(wgpuDeviceCreateRenderPipeline)\
		ENUM(wgpuDeviceCreateRenderPipelineAsync)\
		ENUM(wgpuDeviceCreateSampler)\
		ENUM(wgpuDeviceCreateShaderModule)\
		ENUM(wgpuDeviceCreateSwapChain)\
		ENUM(wgpuDeviceCreateTexture)\
		ENUM(wgpuDeviceDestroy)\
		ENUM(wgpuDeviceEnumerateFeatures)\
		ENUM(wgpuDeviceForceLoss)\
		ENUM(wgpuDeviceGetAdapter)\
		ENUM(wgpuDeviceGetLimits)\
		ENUM(wgpuDeviceGetQueue)\
		ENUM(wgpuDeviceGetSupportedSurfaceUsage)\
		ENUM(wgpuDeviceHasFeature)\
		ENUM(wgpuDeviceInjectError)\
		ENUM(wgpuDevicePopErrorScope)\
		ENUM(wgpuDevicePushErrorScope)\
		ENUM(wgpuDeviceSetDeviceLostCallback)\
		ENUM(wgpuDeviceSetLabel)\
		ENUM(wgpuDeviceSetLoggingCallback)\
		ENUM(wgpuDeviceSetUncapturedErrorCallback)\
		ENUM(wgpuDeviceTick)\
		ENUM(wgpuDeviceValidateTextureDescriptor)\
		ENUM(wgpuDeviceReference)\
		ENUM(wgpuDeviceRelease)\

		// Methods of ExternalTexture
#define WGPU_FUNCS_EXTERNALTEXTURE(ENUM)  \
		ENUM(wgpuExternalTextureDestroy)\
		ENUM(wgpuExternalTextureExpire)\
		ENUM(wgpuExternalTextureRefresh)\
		ENUM(wgpuExternalTextureSetLabel)\
		ENUM(wgpuExternalTextureReference)\
		ENUM(wgpuExternalTextureRelease)\

		// Methods of Instance
#define WGPU_FUNCS_INSTANCE(ENUM)  \
		ENUM(wgpuInstanceCreateSurface)\
		ENUM(wgpuInstanceProcessEvents)\
		ENUM(wgpuInstanceRequestAdapter)\
		ENUM(wgpuInstanceReference)\
		ENUM(wgpuInstanceRelease)\

		// Methods of PipelineLayout
#define WGPU_FUNCS_PIPELINELAYOUT(ENUM)  \
		ENUM(wgpuPipelineLayoutSetLabel)\
		ENUM(wgpuPipelineLayoutReference)\
		ENUM(wgpuPipelineLayoutRelease)\

		// Methods of QuerySet
#define WGPU_FUNCS_QUERYSET(ENUM)  \
		ENUM(wgpuQuerySetDestroy)\
		ENUM(wgpuQuerySetGetCount)\
		ENUM(wgpuQuerySetGetType)\
		ENUM(wgpuQuerySetSetLabel)\
		ENUM(wgpuQuerySetReference)\
		ENUM(wgpuQuerySetRelease)\

		// Methods of Queue
#define WGPU_FUNCS_QUEUE(ENUM)  \
		ENUM(wgpuQueueCopyExternalTextureForBrowser)\
		ENUM(wgpuQueueCopyTextureForBrowser)\
		ENUM(wgpuQueueOnSubmittedWorkDone)\
		ENUM(wgpuQueueSetLabel)\
		ENUM(wgpuQueueSubmit)\
		ENUM(wgpuQueueWriteBuffer)\
		ENUM(wgpuQueueFillBuffer)\
		ENUM(wgpuQueueWriteTexture)\
		ENUM(wgpuQueueFillTexture)\
		ENUM(wgpuQueueReference)\
		ENUM(wgpuQueueRelease)\

		// Methods of RenderBundle
#define WGPU_FUNCS_RENDERBUNDLE(ENUM)  \
		ENUM(wgpuRenderBundleReference)\
		ENUM(wgpuRenderBundleRelease)\

		// Methods of RenderBundleEncoder
#define WGPU_FUNCS_RENDERBUNDLEENCODER(ENUM)  \
		ENUM(wgpuRenderBundleEncoderDraw)\
		ENUM(wgpuRenderBundleEncoderDrawIndexed)\
		ENUM(wgpuRenderBundleEncoderDrawIndexedIndirect)\
		ENUM(wgpuRenderBundleEncoderDrawIndirect)\
		ENUM(wgpuRenderBundleEncoderFinish)\
		ENUM(wgpuRenderBundleEncoderInsertDebugMarker)\
		ENUM(wgpuRenderBundleEncoderPopDebugGroup)\
		ENUM(wgpuRenderBundleEncoderPushDebugGroup)\
		ENUM(wgpuRenderBundleEncoderSetBindGroup)\
		ENUM(wgpuRenderBundleEncoderSetIndexBuffer)\
		ENUM(wgpuRenderBundleEncoderSetLabel)\
		ENUM(wgpuRenderBundleEncoderSetPipeline)\
		ENUM(wgpuRenderBundleEncoderSetVertexBuffer)\
		ENUM(wgpuRenderBundleEncoderReference)\
		ENUM(wgpuRenderBundleEncoderRelease)\

		// Methods of RenderPassEncoder
#define WGPU_FUNCS_RENDERPASSENCODER(ENUM)  \
		ENUM(wgpuRenderPassEncoderBeginOcclusionQuery)\
		ENUM(wgpuRenderPassEncoderDraw)\
		ENUM(wgpuRenderPassEncoderDrawIndexed)\
		ENUM(wgpuRenderPassEncoderDrawIndexedIndirect)\
		ENUM(wgpuRenderPassEncoderDrawIndirect)\
		ENUM(wgpuRenderPassEncoderEnd)\
		ENUM(wgpuRenderPassEncoderEndOcclusionQuery)\
		ENUM(wgpuRenderPassEncoderExecuteBundles)\
		ENUM(wgpuRenderPassEncoderInsertDebugMarker)\
		ENUM(wgpuRenderPassEncoderPopDebugGroup)\
		ENUM(wgpuRenderPassEncoderPushDebugGroup)\
		ENUM(wgpuRenderPassEncoderSetBindGroup)\
		ENUM(wgpuRenderPassEncoderSetBlendConstant)\
		ENUM(wgpuRenderPassEncoderSetIndexBuffer)\
		ENUM(wgpuRenderPassEncoderSetLabel)\
		ENUM(wgpuRenderPassEncoderSetPipeline)\
		ENUM(wgpuRenderPassEncoderSetScissorRect)\
		ENUM(wgpuRenderPassEncoderSetStencilReference)\
		ENUM(wgpuRenderPassEncoderSetVertexBuffer)\
		ENUM(wgpuRenderPassEncoderSetViewport)\
		ENUM(wgpuRenderPassEncoderWriteTimestamp)\
		ENUM(wgpuRenderPassEncoderReference)\
		ENUM(wgpuRenderPassEncoderRelease)\

		// Methods of RenderPipeline
#define WGPU_FUNCS_RENDERPIPELINE(ENUM)  \
		ENUM(wgpuRenderPipelineGetBindGroupLayout)\
		ENUM(wgpuRenderPipelineSetLabel)\
		ENUM(wgpuRenderPipelineReference)\
		ENUM(wgpuRenderPipelineRelease)\

		// Methods of Sampler
#define WGPU_FUNCS_SAMPLER(ENUM)  \
		ENUM(wgpuSamplerSetLabel)\
		ENUM(wgpuSamplerReference)\
		ENUM(wgpuSamplerRelease)\

		// Methods of ShaderModule
#define WGPU_FUNCS_SHADERMODULE(ENUM)  \
		ENUM(wgpuShaderModuleGetCompilationInfo)\
		ENUM(wgpuShaderModuleSetLabel)\
		ENUM(wgpuShaderModuleReference)\
		ENUM(wgpuShaderModuleRelease)\

		// Methods of Surface
#define WGPU_FUNCS_SURFACE(ENUM)  \
		ENUM(wgpuSurfaceReference)\
		ENUM(wgpuSurfaceRelease)\
		ENUM(wgpuSurfacePresent) \
		ENUM(wgpuSurfaceGetCapabilities) \
		ENUM(wgpuSurfaceConfigure) \
		ENUM(wgpuSurfaceGetCurrentTexture) \
		ENUM(wgpuSurfaceCapabilitiesFreeMembers) \

		// Methods of Texture
#define WGPU_FUNCS_TEXTURE(ENUM)  \
		ENUM(wgpuTextureCreateView)\
		ENUM(wgpuTextureDestroy)\
		ENUM(wgpuTextureGetDepthOrArrayLayers)\
		ENUM(wgpuTextureGetDimension)\
		ENUM(wgpuTextureGetFormat)\
		ENUM(wgpuTextureGetHeight)\
		ENUM(wgpuTextureGetMipLevelCount)\
		ENUM(wgpuTextureGetSampleCount)\
		ENUM(wgpuTextureGetUsage)\
		ENUM(wgpuTextureGetWidth)\
		ENUM(wgpuTextureSetLabel)\
		ENUM(wgpuTextureReference)\
		ENUM(wgpuTextureRelease)\

		// Methods of TextureView
#define WGPU_FUNCS_TEXTUREVIEW(ENUM)  \
		ENUM(wgpuTextureViewSetLabel)\
		ENUM(wgpuTextureViewReference)\
		ENUM(wgpuTextureViewRelease)\

		// Misc Methods
#define WGPU_FUNCS_MISC(ENUM)  \
		ENUM(startWebGPU)\
		ENUM(syncWebGPU)\
		ENUM(wgpuFlush)\
		ENUM(wgpuFlushStart)\
		ENUM(wgpuFlushEnd)\
		ENUM(wgpuFlushStop)

#define WGPU_ALL(ENUM) \
		WGPU_FUNCS(ENUM) \
		WGPU_FUNCS_ADAPTER(ENUM) \
		WGPU_FUNCS_BINDGROUP(ENUM) \
		WGPU_FUNCS_BINDGROUPLAYOUT(ENUM) \
		WGPU_FUNCS_BUFFER(ENUM) \
		WGPU_FUNCS_COMMANDBUFFER(ENUM) \
		WGPU_FUNCS_COMMANDENCODER(ENUM) \
		WGPU_FUNCS_COMPUTEPASSENCODER(ENUM) \
		WGPU_FUNCS_COMPUTEPIPELINE(ENUM) \
		WGPU_FUNCS_DEVICE(ENUM) \
		WGPU_FUNCS_EXTERNALTEXTURE(ENUM) \
		WGPU_FUNCS_INSTANCE(ENUM) \
		WGPU_FUNCS_PIPELINELAYOUT(ENUM) \
		WGPU_FUNCS_QUERYSET(ENUM) \
		WGPU_FUNCS_QUEUE(ENUM) \
		WGPU_FUNCS_RENDERBUNDLE(ENUM) \
		WGPU_FUNCS_RENDERBUNDLEENCODER(ENUM) \
		WGPU_FUNCS_RENDERPASSENCODER(ENUM) \
		WGPU_FUNCS_RENDERPIPELINE(ENUM) \
		WGPU_FUNCS_SAMPLER(ENUM) \
		WGPU_FUNCS_SHADERMODULE(ENUM) \
		WGPU_FUNCS_SURFACE(ENUM) \
		WGPU_FUNCS_TEXTURE(ENUM) \
		WGPU_FUNCS_TEXTUREVIEW(ENUM) \
		WGPU_FUNCS_MISC(ENUM)

#define WGPU_CUSTOM(ENUM) \
		ENUM(wgpuBufferGetMappedRangeAndWrite) \
		ENUM(wgpuBufferGetMappedRangeAndFill) \
		ENUM(wgpuQueueWriteBuffer2) \


#define ENUM_COMMA(f) f,
#define WGPU_ITEM(f) {wndr::ID::f, #f},
#define WGPU_ID_TO_STRING(f) #f,
#define WGPU_DEEPCOPY_FORMATS(ENUM)				\
	ENUM(WGPUVertexBufferLayout)				\
	ENUM(WGPUTextureBindingLayout)				\
	ENUM(WGPUStorageTextureBindingLayout)		\
	ENUM(WGPUBufferBindingLayout)				\
	ENUM(WGPUBufferDescriptor)					\
	ENUM(WGPUSamplerBindingLayout)				\
	ENUM(WGPUBindGroupLayoutEntry)				\
	ENUM(WGPUBindGroupLayoutDescriptor)			\
	ENUM(WGPUInstanceDescriptor)				\
	ENUM(WGPUTextureDataLayout)					\
	ENUM(WGPURenderPassDepthStencilAttachment)	\
	ENUM(WGPUComputeState)						\
	ENUM(WGPUVertexState)						\
	ENUM(WGPUPrimitiveState)					\
	ENUM(WGPUMultisampleState)					\
	ENUM(WGPUDepthStencilState)					\
	ENUM(WGPUColorTargetState)					\
	ENUM(WGPUVertexAttribute)					\
	ENUM(WGPUFragmentState)						\
	ENUM(WGPUConstantEntry)						\
	ENUM(WGPUAdapterProperties)					\
	ENUM(WGPUPipelineLayoutDescriptor)			\
	ENUM(WGPUDeviceDescriptor)					\
	ENUM(WGPUTextureViewDescriptor)				\
	ENUM(WGPUTextureDescriptor)					\
	ENUM(WGPUShaderModuleDescriptor)			\
	ENUM(WGPUShaderModuleWGSLDescriptor)		\
	ENUM(WGPURenderPipelineDescriptor)			\
	ENUM(WGPUBindGroupDescriptor)				\
	ENUM(WGPURenderPassDescriptor)				\
	ENUM(WGPURenderPassColorAttachment)			\
	ENUM(WGPURenderBundleDescriptor)			\
	ENUM(WGPURenderBundleEncoderDescriptor)		\
	ENUM(WGPUComputePipelineDescriptor)			\
	ENUM(WGPUComputePassDescriptor)				\
	ENUM(WGPUSamplerDescriptor)					\
	ENUM(WGPUSurfaceDescriptor)					\
	ENUM(WGPUImageCopyBuffer)					\
	ENUM(WGPUImageCopyTexture)					\
	ENUM(WGPUExtent3D)							\
	ENUM(WGPUQuerySetDescriptor)				\
	ENUM(WGPUCommandEncoderDescriptor)			\
	ENUM(WGPUCommandBufferDescriptor)			\

// all types minus WGPUInstance, WGPUQueue, WGPUDevice, because they are not created by the user
#define WGPU_TYPES(ENUM) \
	ENUM(WGPUBindGroup) \
	ENUM(WGPUBindGroupLayout) \
	ENUM(WGPUBuffer) \
	ENUM(WGPUCommandBuffer) \
	ENUM(WGPUCommandEncoder) \
	ENUM(WGPUComputePassEncoder) \
	ENUM(WGPUComputePipeline) \
	ENUM(WGPUPipelineLayout) \
	ENUM(WGPUQuerySet) \
	ENUM(WGPURenderBundle) \
	ENUM(WGPURenderBundleEncoder) \
	ENUM(WGPURenderPassEncoder) \
	ENUM(WGPURenderPipeline) \
	ENUM(WGPUSampler) \
	ENUM(WGPUShaderModule) \
	ENUM(WGPUTexture) \
	ENUM(WGPUTextureView) \
	ENUM(WGPUSurface)

#define OTHER_FUNCS(ENUM)               \
	ENUM(MouseMove)                     \
	ENUM(MouseRelative)                 \
	ENUM(MousePosition)                 \
	ENUM(MouseScroll)					\
	ENUM(LMouseDown)                    \
	ENUM(LMouseUp)                      \
	ENUM(RMouseDown)                    \
	ENUM(RMouseUp)                      \
	ENUM(KeyChar)                       \
	ENUM(KeyDown)                       \
	ENUM(KeyUp)                         \
	ENUM(enableUniformCaching)          \
	ENUM(disableUniformCaching)			\
	ENUM(SetCursor)						\
	ENUM(endOfFrame)					\
	ENUM(syncGFX)						\
	ENUM(ReshapeWindow)					\
	ENUM(wgpuQueueWriteBuffer_cb)		\
	ENUM(wgpuQueueWriteTexture_cb)		\

#define WGPU_CREATE_SIMPLETYPE_ARCHIVE(type) \
FORCEINLINE_DEBUGGABLE Archive& operator>>(Archive& ar, type& v) { u64 a; ar >> a; v = (type)(uintptr_t)a; return ar; } \
FORCEINLINE_DEBUGGABLE Archive& operator<<(Archive& ar, const type& v) { ar << (u64)(uintptr_t)v; return ar; } \
FORCEINLINE_DEBUGGABLE Archive& operator<<(Archive& ar, const type* v) { ar << (u64)(uintptr_t)*v; return ar; }

#define WGPU_PROCESSARG(StructType) \
	[[nodiscard]] FORCEINLINE_DEBUGGABLE StructType ProcessArg(StructType arg) { \
		return static_cast<StructType>(IDPool::get().getResource(arg)); \
	}

// special methods - find out how we can make these work with our macro system
//wgpuCreateInstance
//wgpuGetProcAddress
//wgpuAdapterCreateDevice
//wgpuAdapterEnumerateFeatures
//wgpuAdapterGetInstance
//wgpuBindGroupRelease
//wgpuBufferGetConstMappedRange
//wgpuBufferGetMappedRange
//wgpuBufferMapAsync
//wgpuBufferUnmap
//wgpuBufferRelease
//wgpuCommandBufferRelease
//wgpuCommandEncoderRelease
//wgpuComputePassEncoderRelease
//wgpuRenderPassEncoderRelease
//wgpuTextureRelease
//wgpuTextureViewRelease
//wgpuQueueReference
//wgpuQueueOnSubmittedWorkDone
//wgpuDeviceGetQueue
//wgpuDeviceGetLimits
//wgpuComputePassEncoderSetBindGroup
//wgpuInstanceRequestAdapter
//wgpuRenderPassEncoderSetBindGroup
//wgpuSwapChainPresent
//wgpuFlush
// these unimplemented are for method and remote
#define WGPU_UNIMPLEMENTED_SILENT(ENUM)
#define WGPU_UNIMPLEMENTED(ENUM) \
	ENUM(wgpuBufferGetMapState) \
	ENUM(wgpuBufferGetSize) \
	ENUM(wgpuBufferGetUsage) \
	ENUM(wgpuCommandEncoderCopyTextureToTextureInternal) \
	ENUM(wgpuCommandEncoderInjectValidationError) \
	ENUM(wgpuCommandEncoderWriteBuffer) \
	ENUM(wgpuDeviceCreateComputePipelineAsync) \
	ENUM(wgpuDeviceCreateErrorBuffer) \
	ENUM(wgpuDeviceCreateErrorExternalTexture) \
	ENUM(wgpuDeviceCreateExternalTexture) \
	ENUM(wgpuDeviceCreateErrorTexture) \
	ENUM(wgpuDeviceCreateRenderPipelineAsync) \
	ENUM(wgpuRenderBundleEncoderSetBindGroup) \
	ENUM(wgpuDeviceSetDeviceLostCallback) \
	ENUM(wgpuDeviceSetLoggingCallback) \
	ENUM(wgpuDeviceSetUncapturedErrorCallback) \
	ENUM(wgpuDeviceTick) \
	ENUM(wgpuDeviceGetAdapter) \
	ENUM(wgpuDeviceEnumerateFeatures) \
	ENUM(wgpuDeviceForceLoss) \
	ENUM(wgpuDeviceGetSupportedSurfaceUsage) \
	ENUM(wgpuDeviceInjectError) \
	ENUM(wgpuExternalTextureDestroy) \
	ENUM(wgpuExternalTextureExpire) \
	ENUM(wgpuExternalTextureRefresh) \
	ENUM(wgpuExternalTextureSetLabel) \
	ENUM(wgpuExternalTextureReference) \
	ENUM(wgpuExternalTextureRelease) \
	ENUM(wgpuDeviceValidateTextureDescriptor) \
	ENUM(wgpuDeviceTick) \
	ENUM(wgpuQueueCopyTextureForBrowser) \
	ENUM(wgpuQueueCopyExternalTextureForBrowser) \
	ENUM(wgpuDevicePopErrorScope) \
	ENUM(wgpuQuerySetGetCount) \
	ENUM(wgpuQuerySetGetType) \
	ENUM(wgpuTextureGetDepthOrArrayLayers) \
	ENUM(wgpuTextureGetDimension) \
	ENUM(wgpuTextureGetFormat) \
	ENUM(wgpuTextureGetHeight) \
	ENUM(wgpuTextureGetMipLevelCount) \
	ENUM(wgpuTextureGetSampleCount) \
	ENUM(wgpuTextureGetUsage) \
	ENUM(wgpuTextureGetWidth) \
	ENUM(wgpuRenderPassEncoderSetBlendConstant) \
	ENUM(wgpuShaderModuleGetCompilationInfo)
#define WGPU_ASYNC_NORET_SHARED(ENUM) \
	ENUM(wgpuAdapterReference) \
	ENUM(wgpuAdapterRelease) \
	ENUM(wgpuBindGroupSetLabel) \
	ENUM(wgpuBindGroupReference) \
	ENUM(wgpuBindGroupLayoutSetLabel) \
	ENUM(wgpuBindGroupLayoutReference) \
	ENUM(wgpuBindGroupLayoutRelease) \
	ENUM(wgpuBufferDestroy) \
	ENUM(wgpuBufferSetLabel) \
	ENUM(wgpuBufferReference) \
	ENUM(wgpuCommandBufferSetLabel) \
	ENUM(wgpuCommandBufferReference) \
	ENUM(wgpuCommandEncoderClearBuffer) \
	ENUM(wgpuCommandEncoderCopyBufferToBuffer) \
	ENUM(wgpuCommandEncoderCopyBufferToTexture) \
	ENUM(wgpuCommandEncoderCopyTextureToBuffer) \
	ENUM(wgpuCommandEncoderCopyTextureToTexture) \
	ENUM(wgpuCommandEncoderInsertDebugMarker) \
	ENUM(wgpuCommandEncoderPopDebugGroup) \
	ENUM(wgpuCommandEncoderPushDebugGroup) \
	ENUM(wgpuCommandEncoderResolveQuerySet) \
	ENUM(wgpuCommandEncoderSetLabel) \
	ENUM(wgpuCommandEncoderWriteTimestamp) \
	ENUM(wgpuCommandEncoderReference) \
	ENUM(wgpuComputePassEncoderDispatchWorkgroups) \
	ENUM(wgpuComputePassEncoderDispatchWorkgroupsIndirect) \
	ENUM(wgpuComputePassEncoderEnd) \
	ENUM(wgpuComputePassEncoderInsertDebugMarker) \
	ENUM(wgpuComputePassEncoderPopDebugGroup) \
	ENUM(wgpuComputePassEncoderPushDebugGroup) \
	ENUM(wgpuComputePassEncoderSetLabel) \
	ENUM(wgpuComputePassEncoderSetPipeline) \
	ENUM(wgpuComputePassEncoderWriteTimestamp) \
	ENUM(wgpuComputePassEncoderReference) \
	ENUM(wgpuComputePipelineSetLabel) \
	ENUM(wgpuComputePipelineReference) \
	ENUM(wgpuComputePipelineRelease) \
	ENUM(wgpuComputePipelineGetBindGroupLayout) \
	ENUM(wgpuDeviceDestroy) \
	ENUM(wgpuDevicePushErrorScope) \
	ENUM(wgpuDeviceSetLabel) \
	ENUM(wgpuDeviceReference) \
	ENUM(wgpuDeviceRelease) \
	ENUM(wgpuInstanceReference) \
	ENUM(wgpuInstanceRelease) \
	ENUM(wgpuPipelineLayoutSetLabel) \
	ENUM(wgpuPipelineLayoutReference) \
	ENUM(wgpuPipelineLayoutRelease) \
	ENUM(wgpuQuerySetDestroy) \
	ENUM(wgpuQuerySetSetLabel) \
	ENUM(wgpuQuerySetReference) \
	ENUM(wgpuQuerySetRelease) \
	ENUM(wgpuQueueSetLabel) \
	ENUM(wgpuQueueSubmit) \
	ENUM(wgpuQueueWriteBuffer) \
	ENUM(wgpuQueueWriteTexture) \
	ENUM(wgpuQueueRelease) \
	ENUM(wgpuRenderBundleReference) \
	ENUM(wgpuRenderBundleRelease) \
	ENUM(wgpuRenderBundleEncoderDraw) \
	ENUM(wgpuRenderBundleEncoderDrawIndexed) \
	ENUM(wgpuRenderBundleEncoderDrawIndexedIndirect) \
	ENUM(wgpuRenderBundleEncoderDrawIndirect) \
	ENUM(wgpuRenderBundleEncoderInsertDebugMarker) \
	ENUM(wgpuRenderBundleEncoderPopDebugGroup) \
	ENUM(wgpuRenderBundleEncoderPushDebugGroup) \
	ENUM(wgpuRenderBundleEncoderSetIndexBuffer) \
	ENUM(wgpuRenderBundleEncoderSetLabel) \
	ENUM(wgpuRenderBundleEncoderSetPipeline) \
	ENUM(wgpuRenderBundleEncoderSetVertexBuffer) \
	ENUM(wgpuRenderBundleEncoderReference) \
	ENUM(wgpuRenderBundleEncoderRelease) \
	ENUM(wgpuRenderPassEncoderBeginOcclusionQuery) \
	ENUM(wgpuRenderPassEncoderDraw) \
	ENUM(wgpuRenderPassEncoderDrawIndexed) \
	ENUM(wgpuRenderPassEncoderDrawIndexedIndirect) \
	ENUM(wgpuRenderPassEncoderDrawIndirect) \
	ENUM(wgpuRenderPassEncoderEnd) \
	ENUM(wgpuRenderPassEncoderEndOcclusionQuery) \
	ENUM(wgpuRenderPassEncoderExecuteBundles) \
	ENUM(wgpuRenderPassEncoderInsertDebugMarker) \
	ENUM(wgpuRenderPassEncoderPopDebugGroup) \
	ENUM(wgpuRenderPassEncoderPushDebugGroup) \
	ENUM(wgpuRenderPassEncoderSetIndexBuffer) \
	ENUM(wgpuRenderPassEncoderSetLabel) \
	ENUM(wgpuRenderPassEncoderSetPipeline) \
	ENUM(wgpuRenderPassEncoderSetScissorRect) \
	ENUM(wgpuRenderPassEncoderSetStencilReference) \
	ENUM(wgpuRenderPassEncoderSetVertexBuffer) \
	ENUM(wgpuRenderPassEncoderSetViewport) \
	ENUM(wgpuRenderPassEncoderWriteTimestamp) \
	ENUM(wgpuRenderPassEncoderReference) \
	ENUM(wgpuRenderPipelineSetLabel) \
	ENUM(wgpuRenderPipelineReference) \
	ENUM(wgpuRenderPipelineRelease) \
	ENUM(wgpuSamplerSetLabel) \
	ENUM(wgpuSamplerReference) \
	ENUM(wgpuSamplerRelease) \
	ENUM(wgpuShaderModuleSetLabel) \
	ENUM(wgpuShaderModuleReference) \
	ENUM(wgpuShaderModuleRelease) \
	ENUM(wgpuSurfaceReference) \
	ENUM(wgpuSurfaceRelease) \
	ENUM(wgpuSurfacePresent) \
	ENUM(wgpuSurfaceConfigure) \
	ENUM(wgpuSurfaceGetCapabilities) \
	ENUM(wgpuSurfaceGetCurrentTexture) \
	ENUM(wgpuSurfaceCapabilitiesFreeMembers) \
	ENUM(wgpuTextureDestroy) \
	ENUM(wgpuTextureSetLabel) \
	ENUM(wgpuTextureReference) \
	ENUM(wgpuTextureViewSetLabel) \
	ENUM(wgpuTextureViewReference)
#define WGPU_ASYNC_RET_SHARED(ENUM) \
	ENUM(wgpuCommandEncoderBeginComputePass) \
	ENUM(wgpuCommandEncoderBeginRenderPass) \
	ENUM(wgpuCommandEncoderFinish) \
	ENUM(wgpuRenderBundleEncoderFinish) \
	ENUM(wgpuDeviceCreateBindGroup) \
	ENUM(wgpuDeviceCreateBindGroupLayout) \
	ENUM(wgpuDeviceCreateBuffer) \
	ENUM(wgpuDeviceCreateCommandEncoder) \
	ENUM(wgpuDeviceCreateComputePipeline) \
	ENUM(wgpuDeviceCreatePipelineLayout) \
	ENUM(wgpuDeviceCreateRenderBundleEncoder) \
	ENUM(wgpuDeviceCreateRenderPipeline) \
	ENUM(wgpuDeviceCreateSampler) \
	ENUM(wgpuDeviceCreateShaderModule) \
	ENUM(wgpuDeviceCreateSwapChain) \
	ENUM(wgpuDeviceCreateTexture) \
	ENUM(wgpuInstanceCreateSurface) \
	ENUM(wgpuSwapChainGetCurrentTextureView) \
	ENUM(wgpuTextureCreateView) \
	ENUM(wgpuRenderPipelineGetBindGroupLayout)
#define WGPU_SYNC_RET_TEMP_SHARED(ENUM) \
	EMUM(wgpuDeviceHasFeature)
#if WGPU_PROXY_TYPE == WGPU_PROXY_NONE
#undef WGPU_UNIMPLEMENTED
#define WGPU_UNIMPLEMENTED(ENUM)
#define WGPU_ASYNC_NORET(ENUM)
#define WGPU_SYNC_RET(ENUM) ENUM(WGPU_ALL)
#define WGPU_SYNC_RET_TEMP(ENUM)
#define WGPU_EMPTY(ENUM)
#elif WGPU_PROXY_TYPE == WGPU_PROXY_METHOD
#define WGPU_ASYNC_NORET(ENUM) \
				WGPU_ASYNC_NORET_SHARED(ENUM) \
				ENUM(wgpuDeviceCreateQuerySet) \
				ENUM(wgpuInstanceProcessEvents)
#define WGPU_SYNC_RET(ENUM) \
				ENUM(wgpuAdapterGetLimits) \
				ENUM(wgpuAdapterGetInfo) \
				ENUM(wgpuAdapterHasFeature)
#define WGPU_SYNC_RET_TEMP(ENUM) \
				ENUM(wgpuAdapterEnumerateFeatures)
#define WGPU_EMPTY(ENUM) \
				ENUM(wgpuAdapterRequestDevice)
#else
#define WGPU_ASYNC_NORET(ENUM) \
				WGPU_ASYNC_NORET_SHARED(ENUM)
#define WGPU_SYNC_RET(ENUM) \
				ENUM(wgpuAdapterGetLimits) \
				ENUM(wgpuAdapterGetInfo) \
				ENUM(wgpuAdapterHasFeature)
#define WGPU_SYNC_RET_TEMP(ENUM)
#define WGPU_EMPTY(ENUM) \
				ENUM(wgpuAdapterEnumerateFeatures) \
				ENUM(wgpuAdapterRequestDevice)
#endif