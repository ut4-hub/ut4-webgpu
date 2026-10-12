#if defined(OVERRIDE_WEBGPU)
#define wgpuCreateInstance wndr::wgpuCreateInstance
#define wgpuGetProcAddress wndr::wgpuGetProcAddress

// Methods of Adapter
#define wgpuAdapterCreateDevice wndr::wgpuAdapterCreateDevice
#define wgpuAdapterEnumerateFeatures wndr::wgpuAdapterEnumerateFeatures
#define wgpuAdapterGetInstance wndr::wgpuAdapterGetInstance
#define wgpuAdapterGetLimits wndr::wgpuAdapterGetLimits
#define wgpuAdapterGetInfo wndr::wgpuAdapterGetInfo
#define wgpuAdapterHasFeature wndr::wgpuAdapterHasFeature
#define wgpuAdapterRequestDevice wndr::wgpuAdapterRequestDevice
#define wgpuAdapterAddRef wndr::wgpuAdapterAddRef
#define wgpuAdapterRelease wndr::wgpuAdapterRelease

// Methods of BindGroup
#define wgpuBindGroupSetLabel wndr::wgpuBindGroupSetLabel
#define wgpuBindGroupAddRef wndr::wgpuBindGroupAddRef
#define wgpuBindGroupRelease wndr::wgpuBindGroupRelease

// Methods of BindGroupLayout
#define wgpuBindGroupLayoutSetLabel wndr::wgpuBindGroupLayoutSetLabel
#define wgpuBindGroupLayoutAddRef wndr::wgpuBindGroupLayoutAddRef
#define wgpuBindGroupLayoutRelease wndr::wgpuBindGroupLayoutRelease

// Methods of Buffer
#define wgpuBufferDestroy wndr::wgpuBufferDestroy
#define wgpuBufferGetConstMappedRange wndr::wgpuBufferGetConstMappedRange
#define wgpuBufferGetMapState wndr::wgpuBufferGetMapState
#define wgpuBufferGetMappedRange wndr::wgpuBufferGetMappedRange
#define wgpuBufferGetSize wndr::wgpuBufferGetSize
#define wgpuBufferGetUsage wndr::wgpuBufferGetUsage
#define wgpuBufferMapAsync wndr::wgpuBufferMapAsync
#define wgpuBufferSetLabel wndr::wgpuBufferSetLabel
#define wgpuBufferUnmap wndr::wgpuBufferUnmap
#define wgpuBufferAddRef wndr::wgpuBufferAddRef
#define wgpuBufferRelease wndr::wgpuBufferRelease

// Methods of CommandBuffer
#define wgpuCommandBufferSetLabel wndr::wgpuCommandBufferSetLabel
#define wgpuCommandBufferAddRef wndr::wgpuCommandBufferAddRef
#define wgpuCommandBufferRelease wndr::wgpuCommandBufferRelease

// Methods of CommandEncoder
#define wgpuCommandEncoderBeginComputePass wndr::wgpuCommandEncoderBeginComputePass
#define wgpuCommandEncoderBeginRenderPass wndr::wgpuCommandEncoderBeginRenderPass
#define wgpuCommandEncoderClearBuffer wndr::wgpuCommandEncoderClearBuffer
#define wgpuCommandEncoderCopyBufferToBuffer wndr::wgpuCommandEncoderCopyBufferToBuffer
#define wgpuCommandEncoderCopyBufferToTexture wndr::wgpuCommandEncoderCopyBufferToTexture
#define wgpuCommandEncoderCopyTextureToBuffer wndr::wgpuCommandEncoderCopyTextureToBuffer
#define wgpuCommandEncoderCopyTextureToTexture wndr::wgpuCommandEncoderCopyTextureToTexture
#define wgpuCommandEncoderCopyTextureToTextureInternal wndr::wgpuCommandEncoderCopyTextureToTextureInternal
#define wgpuCommandEncoderFinish wndr::wgpuCommandEncoderFinish
#define wgpuCommandEncoderInjectValidationError wndr::wgpuCommandEncoderInjectValidationError
#define wgpuCommandEncoderInsertDebugMarker wndr::wgpuCommandEncoderInsertDebugMarker
#define wgpuCommandEncoderPopDebugGroup wndr::wgpuCommandEncoderPopDebugGroup
#define wgpuCommandEncoderPushDebugGroup wndr::wgpuCommandEncoderPushDebugGroup
#define wgpuCommandEncoderResolveQuerySet wndr::wgpuCommandEncoderResolveQuerySet
#define wgpuCommandEncoderSetLabel wndr::wgpuCommandEncoderSetLabel
#define wgpuCommandEncoderWriteBuffer wndr::wgpuCommandEncoderWriteBuffer
#define wgpuCommandEncoderWriteTimestamp wndr::wgpuCommandEncoderWriteTimestamp
#define wgpuCommandEncoderAddRef wndr::wgpuCommandEncoderAddRef
#define wgpuCommandEncoderRelease wndr::wgpuCommandEncoderRelease

// Methods of ComputePassEncoder
#define wgpuComputePassEncoderDispatchWorkgroups wndr::wgpuComputePassEncoderDispatchWorkgroups
#define wgpuComputePassEncoderDispatchWorkgroupsIndirect wndr::wgpuComputePassEncoderDispatchWorkgroupsIndirect
#define wgpuComputePassEncoderEnd wndr::wgpuComputePassEncoderEnd
#define wgpuComputePassEncoderInsertDebugMarker wndr::wgpuComputePassEncoderInsertDebugMarker
#define wgpuComputePassEncoderPopDebugGroup wndr::wgpuComputePassEncoderPopDebugGroup
#define wgpuComputePassEncoderPushDebugGroup wndr::wgpuComputePassEncoderPushDebugGroup
#define wgpuComputePassEncoderSetBindGroup wndr::wgpuComputePassEncoderSetBindGroup
#define wgpuComputePassEncoderSetLabel wndr::wgpuComputePassEncoderSetLabel
#define wgpuComputePassEncoderSetPipeline wndr::wgpuComputePassEncoderSetPipeline
#define wgpuComputePassEncoderWriteTimestamp wndr::wgpuComputePassEncoderWriteTimestamp
#define wgpuComputePassEncoderAddRef wndr::wgpuComputePassEncoderAddRef
#define wgpuComputePassEncoderRelease wndr::wgpuComputePassEncoderRelease

// Methods of ComputePipeline
#define wgpuComputePipelineGetBindGroupLayout wndr::wgpuComputePipelineGetBindGroupLayout
#define wgpuComputePipelineSetLabel wndr::wgpuComputePipelineSetLabel
#define wgpuComputePipelineAddRef wndr::wgpuComputePipelineAddRef
#define wgpuComputePipelineRelease wndr::wgpuComputePipelineRelease

// Methods of Device
#define wgpuDeviceCreateBindGroup wndr::wgpuDeviceCreateBindGroup
#define wgpuDeviceCreateBindGroupLayout wndr::wgpuDeviceCreateBindGroupLayout
#define wgpuDeviceCreateBuffer wndr::wgpuDeviceCreateBuffer
#define wgpuDeviceCreateCommandEncoder wndr::wgpuDeviceCreateCommandEncoder
#define wgpuDeviceCreateComputePipeline wndr::wgpuDeviceCreateComputePipeline
#define wgpuDeviceCreateComputePipelineAsync wndr::wgpuDeviceCreateComputePipelineAsync
#define wgpuDeviceCreateErrorBuffer wndr::wgpuDeviceCreateErrorBuffer
#define wgpuDeviceCreateErrorExternalTexture wndr::wgpuDeviceCreateErrorExternalTexture
#define wgpuDeviceCreateErrorTexture wndr::wgpuDeviceCreateErrorTexture
#define wgpuDeviceCreateExternalTexture wndr::wgpuDeviceCreateExternalTexture
#define wgpuDeviceCreatePipelineLayout wndr::wgpuDeviceCreatePipelineLayout
#define wgpuDeviceCreateQuerySet wndr::wgpuDeviceCreateQuerySet
#define wgpuDeviceCreateRenderBundleEncoder wndr::wgpuDeviceCreateRenderBundleEncoder
#define wgpuDeviceCreateRenderPipeline wndr::wgpuDeviceCreateRenderPipeline
#define wgpuDeviceCreateRenderPipelineAsync wndr::wgpuDeviceCreateRenderPipelineAsync
#define wgpuDeviceCreateSampler wndr::wgpuDeviceCreateSampler
#define wgpuDeviceCreateShaderModule wndr::wgpuDeviceCreateShaderModule
#define wgpuDeviceCreateTexture wndr::wgpuDeviceCreateTexture
#define wgpuDeviceDestroy wndr::wgpuDeviceDestroy
#define wgpuDeviceEnumerateFeatures wndr::wgpuDeviceEnumerateFeatures
#define wgpuDeviceForceLoss wndr::wgpuDeviceForceLoss
#define wgpuDeviceGetAdapter wndr::wgpuDeviceGetAdapter
#define wgpuDeviceGetLimits wndr::wgpuDeviceGetLimits
#define wgpuDeviceGetQueue wndr::wgpuDeviceGetQueue
#define wgpuDeviceGetSupportedSurfaceUsage wndr::wgpuDeviceGetSupportedSurfaceUsage
#define wgpuDeviceHasFeature wndr::wgpuDeviceHasFeature
#define wgpuDeviceInjectError wndr::wgpuDeviceInjectError
#define wgpuDevicePopErrorScope wndr::wgpuDevicePopErrorScope
#define wgpuDevicePushErrorScope wndr::wgpuDevicePushErrorScope
#define wgpuDeviceSetDeviceLostCallback wndr::wgpuDeviceSetDeviceLostCallback
#define wgpuDeviceSetLabel wndr::wgpuDeviceSetLabel
#define wgpuDeviceSetLoggingCallback wndr::wgpuDeviceSetLoggingCallback
#define wgpuDeviceSetUncapturedErrorCallback wndr::wgpuDeviceSetUncapturedErrorCallback
#define wgpuDeviceTick wndr::wgpuDeviceTick
#define wgpuDeviceValidateTextureDescriptor wndr::wgpuDeviceValidateTextureDescriptor
#define wgpuDeviceAddRef wndr::wgpuDeviceAddRef
#define wgpuDeviceRelease wndr::wgpuDeviceRelease

// Methods of ExternalTexture
#define wgpuExternalTextureDestroy wndr::wgpuExternalTextureDestroy
#define wgpuExternalTextureExpire wndr::wgpuExternalTextureExpire
#define wgpuExternalTextureRefresh wndr::wgpuExternalTextureRefresh
#define wgpuExternalTextureSetLabel wndr::wgpuExternalTextureSetLabel
#define wgpuExternalTextureAddRef wndr::wgpuExternalTextureAddRef
#define wgpuExternalTextureRelease wndr::wgpuExternalTextureRelease

// Methods of Instance
#define wgpuInstanceCreateSurface wndr::wgpuInstanceCreateSurface
#define wgpuInstanceProcessEvents wndr::wgpuInstanceProcessEvents
#define wgpuInstanceRequestAdapter wndr::wgpuInstanceRequestAdapter
#define wgpuInstanceAddRef wndr::wgpuInstanceAddRef
#define wgpuInstanceRelease wndr::wgpuInstanceRelease

// Methods of PipelineLayout
#define wgpuPipelineLayoutSetLabel wndr::wgpuPipelineLayoutSetLabel
#define wgpuPipelineLayoutAddRef wndr::wgpuPipelineLayoutAddRef
#define wgpuPipelineLayoutRelease wndr::wgpuPipelineLayoutRelease

// Methods of QuerySet
#define wgpuQuerySetDestroy wndr::wgpuQuerySetDestroy
#define wgpuQuerySetGetCount wndr::wgpuQuerySetGetCount
#define wgpuQuerySetGetType wndr::wgpuQuerySetGetType
#define wgpuQuerySetSetLabel wndr::wgpuQuerySetSetLabel
#define wgpuQuerySetAddRef wndr::wgpuQuerySetAddRef
#define wgpuQuerySetRelease wndr::wgpuQuerySetRelease

// Methods of Queue
#define wgpuQueueCopyExternalTextureForBrowser wndr::wgpuQueueCopyExternalTextureForBrowser
#define wgpuQueueCopyTextureForBrowser wndr::wgpuQueueCopyTextureForBrowser
#define wgpuQueueOnSubmittedWorkDone wndr::wgpuQueueOnSubmittedWorkDone
#define wgpuQueueSetLabel wndr::wgpuQueueSetLabel
#define wgpuQueueSubmit wndr::wgpuQueueSubmit
#define wgpuQueueWriteBuffer wndr::wgpuQueueWriteBuffer
#define wgpuQueueWriteBuffer2 wndr::wgpuQueueWriteBuffer2
#define wgpuQueueWriteTexture wndr::wgpuQueueWriteTexture
#define wgpuQueueAddRef wndr::wgpuQueueAddRef
#define wgpuQueueRelease wndr::wgpuQueueRelease

// Methods of RenderBundle
#define wgpuRenderBundleAddRef wndr::wgpuRenderBundleAddRef
#define wgpuRenderBundleRelease wndr::wgpuRenderBundleRelease

// Methods of RenderBundleEncoder
#define wgpuRenderBundleEncoderDraw wndr::wgpuRenderBundleEncoderDraw
#define wgpuRenderBundleEncoderDrawIndexed wndr::wgpuRenderBundleEncoderDrawIndexed
#define wgpuRenderBundleEncoderDrawIndexedIndirect wndr::wgpuRenderBundleEncoderDrawIndexedIndirect
#define wgpuRenderBundleEncoderDrawIndirect wndr::wgpuRenderBundleEncoderDrawIndirect
#define wgpuRenderBundleEncoderFinish wndr::wgpuRenderBundleEncoderFinish
#define wgpuRenderBundleEncoderInsertDebugMarker wndr::wgpuRenderBundleEncoderInsertDebugMarker
#define wgpuRenderBundleEncoderPopDebugGroup wndr::wgpuRenderBundleEncoderPopDebugGroup
#define wgpuRenderBundleEncoderPushDebugGroup wndr::wgpuRenderBundleEncoderPushDebugGroup
#define wgpuRenderBundleEncoderSetBindGroup wndr::wgpuRenderBundleEncoderSetBindGroup
#define wgpuRenderBundleEncoderSetIndexBuffer wndr::wgpuRenderBundleEncoderSetIndexBuffer
#define wgpuRenderBundleEncoderSetLabel wndr::wgpuRenderBundleEncoderSetLabel
#define wgpuRenderBundleEncoderSetPipeline wndr::wgpuRenderBundleEncoderSetPipeline
#define wgpuRenderBundleEncoderSetVertexBuffer wndr::wgpuRenderBundleEncoderSetVertexBuffer
#define wgpuRenderBundleEncoderAddRef wndr::wgpuRenderBundleEncoderAddRef
#define wgpuRenderBundleEncoderRelease wndr::wgpuRenderBundleEncoderRelease

// Methods of RenderPassEncoder
#define wgpuRenderPassEncoderBeginOcclusionQuery wndr::wgpuRenderPassEncoderBeginOcclusionQuery
#define wgpuRenderPassEncoderDraw wndr::wgpuRenderPassEncoderDraw
#define wgpuRenderPassEncoderDrawIndexed wndr::wgpuRenderPassEncoderDrawIndexed
#define wgpuRenderPassEncoderDrawIndexedIndirect wndr::wgpuRenderPassEncoderDrawIndexedIndirect
#define wgpuRenderPassEncoderDrawIndirect wndr::wgpuRenderPassEncoderDrawIndirect
#define wgpuRenderPassEncoderEnd wndr::wgpuRenderPassEncoderEnd
#define wgpuRenderPassEncoderEndOcclusionQuery wndr::wgpuRenderPassEncoderEndOcclusionQuery
#define wgpuRenderPassEncoderExecuteBundles wndr::wgpuRenderPassEncoderExecuteBundles
#define wgpuRenderPassEncoderInsertDebugMarker wndr::wgpuRenderPassEncoderInsertDebugMarker
#define wgpuRenderPassEncoderPopDebugGroup wndr::wgpuRenderPassEncoderPopDebugGroup
#define wgpuRenderPassEncoderPushDebugGroup wndr::wgpuRenderPassEncoderPushDebugGroup
#define wgpuRenderPassEncoderSetBindGroup wndr::wgpuRenderPassEncoderSetBindGroup
#define wgpuRenderPassEncoderSetBlendConstant wndr::wgpuRenderPassEncoderSetBlendConstant
#define wgpuRenderPassEncoderSetIndexBuffer wndr::wgpuRenderPassEncoderSetIndexBuffer
#define wgpuRenderPassEncoderSetLabel wndr::wgpuRenderPassEncoderSetLabel
#define wgpuRenderPassEncoderSetPipeline wndr::wgpuRenderPassEncoderSetPipeline
#define wgpuRenderPassEncoderSetScissorRect wndr::wgpuRenderPassEncoderSetScissorRect
#define wgpuRenderPassEncoderSetStencilReference wndr::wgpuRenderPassEncoderSetStencilReference
#define wgpuRenderPassEncoderSetVertexBuffer wndr::wgpuRenderPassEncoderSetVertexBuffer
#define wgpuRenderPassEncoderSetViewport wndr::wgpuRenderPassEncoderSetViewport
#define wgpuRenderPassEncoderWriteTimestamp wndr::wgpuRenderPassEncoderWriteTimestamp
#define wgpuRenderPassEncoderAddRef wndr::wgpuRenderPassEncoderAddRef
#define wgpuRenderPassEncoderRelease wndr::wgpuRenderPassEncoderRelease

// Methods of RenderPipeline
#define wgpuRenderPipelineGetBindGroupLayout wndr::wgpuRenderPipelineGetBindGroupLayout
#define wgpuRenderPipelineSetLabel wndr::wgpuRenderPipelineSetLabel
#define wgpuRenderPipelineAddRef wndr::wgpuRenderPipelineAddRef
#define wgpuRenderPipelineRelease wndr::wgpuRenderPipelineRelease

// Methods of Sampler
#define wgpuSamplerSetLabel wndr::wgpuSamplerSetLabel
#define wgpuSamplerAddRef wndr::wgpuSamplerAddRef
#define wgpuSamplerRelease wndr::wgpuSamplerRelease

// Methods of ShaderModule
#define wgpuShaderModuleGetCompilationInfo wndr::wgpuShaderModuleGetCompilationInfo
#define wgpuShaderModuleSetLabel wndr::wgpuShaderModuleSetLabel
#define wgpuShaderModuleAddRef wndr::wgpuShaderModuleAddRef
#define wgpuShaderModuleRelease wndr::wgpuShaderModuleRelease

// Methods of Surface
#define wgpuSurfaceAddRef wndr::wgpuSurfaceAddRef
#define wgpuSurfaceRelease wndr::wgpuSurfaceRelease
#define wgpuSurfacePresent wndr::wgpuSurfacePresent
#define wgpuSurfaceGetCapabilities wndr::wgpuSurfaceGetCapabilities
#define wgpuSurfaceConfigure wndr::wgpuSurfaceConfigure
#define wgpuSurfaceGetCurrentTexture wndr::wgpuSurfaceGetCurrentTexture
#define wgpuSurfaceCapabilitiesFreeMembers wndr::wgpuSurfaceCapabilitiesFreeMembers

// Methods of Texture
#define wgpuTextureCreateView wndr::wgpuTextureCreateView
#define wgpuTextureDestroy wndr::wgpuTextureDestroy
#define wgpuTextureGetDepthOrArrayLayers wndr::wgpuTextureGetDepthOrArrayLayers
#define wgpuTextureGetDimension wndr::wgpuTextureGetDimension
#define wgpuTextureGetFormat wndr::wgpuTextureGetFormat
#define wgpuTextureGetHeight wndr::wgpuTextureGetHeight
#define wgpuTextureGetMipLevelCount wndr::wgpuTextureGetMipLevelCount
#define wgpuTextureGetSampleCount wndr::wgpuTextureGetSampleCount
#define wgpuTextureGetUsage wndr::wgpuTextureGetUsage
#define wgpuTextureGetWidth wndr::wgpuTextureGetWidth
#define wgpuTextureSetLabel wndr::wgpuTextureSetLabel
#define wgpuTextureAddRef wndr::wgpuTextureAddRef
#define wgpuTextureRelease wndr::wgpuTextureRelease

// Methods of TextureView
#define wgpuTextureViewSetLabel wndr::wgpuTextureViewSetLabel
#define wgpuTextureViewAddRef wndr::wgpuTextureViewAddRef
#define wgpuTextureViewRelease wndr::wgpuTextureViewRelease
#endif
#if defined(POP_WEBGPU) and defined(OVERRIDE_WEBGPU)
#pragma pop_macro("wgpuCreateInstance")
#pragma pop_macro("wgpuGetProcAddress")

// Methods of Adapter
#pragma pop_macro("wgpuAdapterCreateDevice")
#pragma pop_macro("wgpuAdapterEnumerateFeatures")
#pragma pop_macro("wgpuAdapterGetInstance")
#pragma pop_macro("wgpuAdapterGetLimits")
#pragma pop_macro("wgpuAdapterGetInfo")
#pragma pop_macro("wgpuAdapterHasFeature")
#pragma pop_macro("wgpuAdapterRequestDevice")
#pragma pop_macro("wgpuAdapterAddRef")
#pragma pop_macro("wgpuAdapterRelease")

// Methods of BindGroup
#pragma pop_macro("wgpuBindGroupSetLabel")
#pragma pop_macro("wgpuBindGroupAddRef")
#pragma pop_macro("wgpuBindGroupRelease")

// Methods of BindGroupLayout
#pragma pop_macro("wgpuBindGroupLayoutSetLabel")
#pragma pop_macro("wgpuBindGroupLayoutAddRef")
#pragma pop_macro("wgpuBindGroupLayoutRelease")

// Methods of Buffer
#pragma pop_macro("wgpuBufferDestroy")
#pragma pop_macro("wgpuBufferGetConstMappedRange")
#pragma pop_macro("wgpuBufferGetMapState")
#pragma pop_macro("wgpuBufferGetMappedRange")
#pragma pop_macro("wgpuBufferGetSize")
#pragma pop_macro("wgpuBufferGetUsage")
#pragma pop_macro("wgpuBufferMapAsync")
#pragma pop_macro("wgpuBufferSetLabel")
#pragma pop_macro("wgpuBufferUnmap")
#pragma pop_macro("wgpuBufferAddRef")
#pragma pop_macro("wgpuBufferRelease")

// Methods of CommandBuffer
#pragma pop_macro("wgpuCommandBufferSetLabel")
#pragma pop_macro("wgpuCommandBufferAddRef")
#pragma pop_macro("wgpuCommandBufferRelease")

// Methods of CommandEncoder
#pragma pop_macro("wgpuCommandEncoderBeginComputePass")
#pragma pop_macro("wgpuCommandEncoderBeginRenderPass")
#pragma pop_macro("wgpuCommandEncoderClearBuffer")
#pragma pop_macro("wgpuCommandEncoderCopyBufferToBuffer")
#pragma pop_macro("wgpuCommandEncoderCopyBufferToTexture")
#pragma pop_macro("wgpuCommandEncoderCopyTextureToBuffer")
#pragma pop_macro("wgpuCommandEncoderCopyTextureToTexture")
#pragma pop_macro("wgpuCommandEncoderCopyTextureToTextureInternal")
#pragma pop_macro("wgpuCommandEncoderFinish")
#pragma pop_macro("wgpuCommandEncoderInjectValidationError")
#pragma pop_macro("wgpuCommandEncoderInsertDebugMarker")
#pragma pop_macro("wgpuCommandEncoderPopDebugGroup")
#pragma pop_macro("wgpuCommandEncoderPushDebugGroup")
#pragma pop_macro("wgpuCommandEncoderResolveQuerySet")
#pragma pop_macro("wgpuCommandEncoderSetLabel")
#pragma pop_macro("wgpuCommandEncoderWriteBuffer")
#pragma pop_macro("wgpuCommandEncoderWriteTimestamp")
#pragma pop_macro("wgpuCommandEncoderAddRef")
#pragma pop_macro("wgpuCommandEncoderRelease")

// Methods of ComputePassEncoder
#pragma pop_macro("wgpuComputePassEncoderDispatchWorkgroups")
#pragma pop_macro("wgpuComputePassEncoderDispatchWorkgroupsIndirect")
#pragma pop_macro("wgpuComputePassEncoderEnd")
#pragma pop_macro("wgpuComputePassEncoderInsertDebugMarker")
#pragma pop_macro("wgpuComputePassEncoderPopDebugGroup")
#pragma pop_macro("wgpuComputePassEncoderPushDebugGroup")
#pragma pop_macro("wgpuComputePassEncoderSetBindGroup")
#pragma pop_macro("wgpuComputePassEncoderSetLabel")
#pragma pop_macro("wgpuComputePassEncoderSetPipeline")
#pragma pop_macro("wgpuComputePassEncoderWriteTimestamp")
#pragma pop_macro("wgpuComputePassEncoderAddRef")
#pragma pop_macro("wgpuComputePassEncoderRelease")

// Methods of ComputePipeline
#pragma pop_macro("wgpuComputePipelineGetBindGroupLayout")
#pragma pop_macro("wgpuComputePipelineSetLabel")
#pragma pop_macro("wgpuComputePipelineAddRef")
#pragma pop_macro("wgpuComputePipelineRelease")

// Methods of Device
#pragma pop_macro("wgpuDeviceCreateBindGroup")
#pragma pop_macro("wgpuDeviceCreateBindGroupLayout")
#pragma pop_macro("wgpuDeviceCreateBuffer")
#pragma pop_macro("wgpuDeviceCreateCommandEncoder")
#pragma pop_macro("wgpuDeviceCreateComputePipeline")
#pragma pop_macro("wgpuDeviceCreateComputePipelineAsync")
#pragma pop_macro("wgpuDeviceCreateErrorBuffer")
#pragma pop_macro("wgpuDeviceCreateErrorExternalTexture")
#pragma pop_macro("wgpuDeviceCreateErrorTexture")
#pragma pop_macro("wgpuDeviceCreateExternalTexture")
#pragma pop_macro("wgpuDeviceCreatePipelineLayout")
#pragma pop_macro("wgpuDeviceCreateQuerySet")
#pragma pop_macro("wgpuDeviceCreateRenderBundleEncoder")
#pragma pop_macro("wgpuDeviceCreateRenderPipeline")
#pragma pop_macro("wgpuDeviceCreateRenderPipelineAsync")
#pragma pop_macro("wgpuDeviceCreateSampler")
#pragma pop_macro("wgpuDeviceCreateShaderModule")
#pragma pop_macro("wgpuDeviceCreateTexture")
#pragma pop_macro("wgpuDeviceDestroy")
#pragma pop_macro("wgpuDeviceEnumerateFeatures")
#pragma pop_macro("wgpuDeviceForceLoss")
#pragma pop_macro("wgpuDeviceGetAdapter")
#pragma pop_macro("wgpuDeviceGetLimits")
#pragma pop_macro("wgpuDeviceGetQueue")
#pragma pop_macro("wgpuDeviceGetSupportedSurfaceUsage")
#pragma pop_macro("wgpuDeviceHasFeature")
#pragma pop_macro("wgpuDeviceInjectError")
#pragma pop_macro("wgpuDevicePopErrorScope")
#pragma pop_macro("wgpuDevicePushErrorScope")
#pragma pop_macro("wgpuDeviceSetDeviceLostCallback")
#pragma pop_macro("wgpuDeviceSetLabel")
#pragma pop_macro("wgpuDeviceSetLoggingCallback")
#pragma pop_macro("wgpuDeviceSetUncapturedErrorCallback")
#pragma pop_macro("wgpuDeviceTick")
#pragma pop_macro("wgpuDeviceValidateTextureDescriptor")
#pragma pop_macro("wgpuDeviceAddRef")
#pragma pop_macro("wgpuDeviceRelease")

// Methods of ExternalTexture
#pragma pop_macro("wgpuExternalTextureDestroy")
#pragma pop_macro("wgpuExternalTextureExpire")
#pragma pop_macro("wgpuExternalTextureRefresh")
#pragma pop_macro("wgpuExternalTextureSetLabel")
#pragma pop_macro("wgpuExternalTextureAddRef")
#pragma pop_macro("wgpuExternalTextureRelease")

// Methods of Instance
#pragma pop_macro("wgpuInstanceCreateSurface")
#pragma pop_macro("wgpuInstanceProcessEvents")
#pragma pop_macro("wgpuInstanceRequestAdapter")
#pragma pop_macro("wgpuInstanceAddRef")
#pragma pop_macro("wgpuInstanceRelease")

// Methods of PipelineLayout
#pragma pop_macro("wgpuPipelineLayoutSetLabel")
#pragma pop_macro("wgpuPipelineLayoutAddRef")
#pragma pop_macro("wgpuPipelineLayoutRelease")

// Methods of QuerySet
#pragma pop_macro("wgpuQuerySetDestroy")
#pragma pop_macro("wgpuQuerySetGetCount")
#pragma pop_macro("wgpuQuerySetGetType")
#pragma pop_macro("wgpuQuerySetSetLabel")
#pragma pop_macro("wgpuQuerySetAddRef")
#pragma pop_macro("wgpuQuerySetRelease")

// Methods of Queue
#pragma pop_macro("wgpuQueueCopyExternalTextureForBrowser")
#pragma pop_macro("wgpuQueueCopyTextureForBrowser")
#pragma pop_macro("wgpuQueueOnSubmittedWorkDone")
#pragma pop_macro("wgpuQueueSetLabel")
#pragma pop_macro("wgpuQueueSubmit")
#pragma pop_macro("wgpuQueueWriteBuffer")
#pragma pop_macro("wgpuQueueWriteBuffer2")
#pragma pop_macro("wgpuQueueWriteTexture")
#pragma pop_macro("wgpuQueueAddRef")
#pragma pop_macro("wgpuQueueRelease")

// Methods of RenderBundle
#pragma pop_macro("wgpuRenderBundleAddRef")
#pragma pop_macro("wgpuRenderBundleRelease")

// Methods of RenderBundleEncoder
#pragma pop_macro("wgpuRenderBundleEncoderDraw")
#pragma pop_macro("wgpuRenderBundleEncoderDrawIndexed")
#pragma pop_macro("wgpuRenderBundleEncoderDrawIndexedIndirect")
#pragma pop_macro("wgpuRenderBundleEncoderDrawIndirect")
#pragma pop_macro("wgpuRenderBundleEncoderFinish")
#pragma pop_macro("wgpuRenderBundleEncoderInsertDebugMarker")
#pragma pop_macro("wgpuRenderBundleEncoderPopDebugGroup")
#pragma pop_macro("wgpuRenderBundleEncoderPushDebugGroup")
#pragma pop_macro("wgpuRenderBundleEncoderSetBindGroup")
#pragma pop_macro("wgpuRenderBundleEncoderSetIndexBuffer")
#pragma pop_macro("wgpuRenderBundleEncoderSetLabel")
#pragma pop_macro("wgpuRenderBundleEncoderSetPipeline")
#pragma pop_macro("wgpuRenderBundleEncoderSetVertexBuffer")
#pragma pop_macro("wgpuRenderBundleEncoderAddRef")
#pragma pop_macro("wgpuRenderBundleEncoderRelease")

// Methods of RenderPassEncoder
#pragma pop_macro("wgpuRenderPassEncoderBeginOcclusionQuery")
#pragma pop_macro("wgpuRenderPassEncoderDraw")
#pragma pop_macro("wgpuRenderPassEncoderDrawIndexed")
#pragma pop_macro("wgpuRenderPassEncoderDrawIndexedIndirect")
#pragma pop_macro("wgpuRenderPassEncoderDrawIndirect")
#pragma pop_macro("wgpuRenderPassEncoderEnd")
#pragma pop_macro("wgpuRenderPassEncoderEndOcclusionQuery")
#pragma pop_macro("wgpuRenderPassEncoderExecuteBundles")
#pragma pop_macro("wgpuRenderPassEncoderInsertDebugMarker")
#pragma pop_macro("wgpuRenderPassEncoderPopDebugGroup")
#pragma pop_macro("wgpuRenderPassEncoderPushDebugGroup")
#pragma pop_macro("wgpuRenderPassEncoderSetBindGroup")
#pragma pop_macro("wgpuRenderPassEncoderSetBlendConstant")
#pragma pop_macro("wgpuRenderPassEncoderSetIndexBuffer")
#pragma pop_macro("wgpuRenderPassEncoderSetLabel")
#pragma pop_macro("wgpuRenderPassEncoderSetPipeline")
#pragma pop_macro("wgpuRenderPassEncoderSetScissorRect")
#pragma pop_macro("wgpuRenderPassEncoderSetStencilReference")
#pragma pop_macro("wgpuRenderPassEncoderSetVertexBuffer")
#pragma pop_macro("wgpuRenderPassEncoderSetViewport")
#pragma pop_macro("wgpuRenderPassEncoderWriteTimestamp")
#pragma pop_macro("wgpuRenderPassEncoderAddRef")
#pragma pop_macro("wgpuRenderPassEncoderRelease")

// Methods of RenderPipeline
#pragma pop_macro("wgpuRenderPipelineGetBindGroupLayout")
#pragma pop_macro("wgpuRenderPipelineSetLabel")
#pragma pop_macro("wgpuRenderPipelineAddRef")
#pragma pop_macro("wgpuRenderPipelineRelease")

// Methods of Sampler
#pragma pop_macro("wgpuSamplerSetLabel")
#pragma pop_macro("wgpuSamplerAddRef")
#pragma pop_macro("wgpuSamplerRelease")

// Methods of ShaderModule
#pragma pop_macro("wgpuShaderModuleGetCompilationInfo")
#pragma pop_macro("wgpuShaderModuleSetLabel")
#pragma pop_macro("wgpuShaderModuleAddRef")
#pragma pop_macro("wgpuShaderModuleRelease")

// Methods of Surface
#pragma pop_macro("wgpuSurfaceAddRef")
#pragma pop_macro("wgpuSurfaceRelease")
#pragma pop_macro("wgpuSurfacePresent")
#pragma pop_macro("wgpuSurfaceGetCapabilities")
#pragma pop_macro("wgpuSurfaceConfigure")
#pragma pop_macro("wgpuSurfaceGetCurrentTexture")
#pragma pop_macro("wgpuSurfaceCapabilitiesFreeMembers")

// Methods of Texture
#pragma pop_macro("wgpuTextureCreateView")
#pragma pop_macro("wgpuTextureDestroy")
#pragma pop_macro("wgpuTextureGetDepthOrArrayLayers")
#pragma pop_macro("wgpuTextureGetDimension")
#pragma pop_macro("wgpuTextureGetFormat")
#pragma pop_macro("wgpuTextureGetHeight")
#pragma pop_macro("wgpuTextureGetMipLevelCount")
#pragma pop_macro("wgpuTextureGetSampleCount")
#pragma pop_macro("wgpuTextureGetUsage")
#pragma pop_macro("wgpuTextureGetWidth")
#pragma pop_macro("wgpuTextureSetLabel")
#pragma pop_macro("wgpuTextureAddRef")
#pragma pop_macro("wgpuTextureRelease")

// Methods of TextureView
#pragma pop_macro("wgpuTextureViewSetLabel")
#pragma pop_macro("wgpuTextureViewAddRef")
#pragma pop_macro("wgpuTextureViewRelease")
#elif defined(PUSH_WEBGPU) and defined(OVERRIDE_WEBGPU)
#pragma push_macro("wgpuCreateInstance")
#undef wgpuCreateInstance
#pragma push_macro("wgpuGetProcAddress")
#undef wgpuGetProcAddress

// Methods of Adapter
#pragma push_macro("wgpuAdapterCreateDevice")
#undef wgpuAdapterCreateDevice
#pragma push_macro("wgpuAdapterEnumerateFeatures")
#undef wgpuAdapterEnumerateFeatures
#pragma push_macro("wgpuAdapterGetInstance")
#undef wgpuAdapterGetInstance
#pragma push_macro("wgpuAdapterGetLimits")
#undef wgpuAdapterGetLimits
#pragma push_macro("wgpuAdapterGetInfo")
#undef wgpuAdapterGetInfo
#pragma push_macro("wgpuAdapterHasFeature")
#undef wgpuAdapterHasFeature
#pragma push_macro("wgpuAdapterRequestDevice")
#undef wgpuAdapterRequestDevice
#pragma push_macro("wgpuAdapterAddRef")
#undef wgpuAdapterAddRef
#pragma push_macro("wgpuAdapterRelease")
#undef wgpuAdapterRelease

// Methods of BindGroup
#pragma push_macro("wgpuBindGroupSetLabel")
#undef wgpuBindGroupSetLabel
#pragma push_macro("wgpuBindGroupAddRef")
#undef wgpuBindGroupAddRef
#pragma push_macro("wgpuBindGroupRelease")
#undef wgpuBindGroupRelease

// Methods of BindGroupLayout
#pragma push_macro("wgpuBindGroupLayoutSetLabel")
#undef wgpuBindGroupLayoutSetLabel
#pragma push_macro("wgpuBindGroupLayoutAddRef")
#undef wgpuBindGroupLayoutAddRef
#pragma push_macro("wgpuBindGroupLayoutRelease")
#undef wgpuBindGroupLayoutRelease

// Methods of Buffer
#pragma push_macro("wgpuBufferDestroy")
#undef wgpuBufferDestroy
#pragma push_macro("wgpuBufferGetConstMappedRange")
#undef wgpuBufferGetConstMappedRange
#pragma push_macro("wgpuBufferGetMapState")
#undef wgpuBufferGetMapState
#pragma push_macro("wgpuBufferGetMappedRange")
#undef wgpuBufferGetMappedRange
#pragma push_macro("wgpuBufferGetSize")
#undef wgpuBufferGetSize
#pragma push_macro("wgpuBufferGetUsage")
#undef wgpuBufferGetUsage
#pragma push_macro("wgpuBufferMapAsync")
#undef wgpuBufferMapAsync
#pragma push_macro("wgpuBufferSetLabel")
#undef wgpuBufferSetLabel
#pragma push_macro("wgpuBufferUnmap")
#undef wgpuBufferUnmap
#pragma push_macro("wgpuBufferAddRef")
#undef wgpuBufferAddRef
#pragma push_macro("wgpuBufferRelease")
#undef wgpuBufferRelease

// Methods of CommandBuffer
#pragma push_macro("wgpuCommandBufferSetLabel")
#undef wgpuCommandBufferSetLabel
#pragma push_macro("wgpuCommandBufferAddRef")
#undef wgpuCommandBufferAddRef
#pragma push_macro("wgpuCommandBufferRelease")
#undef wgpuCommandBufferRelease

// Methods of CommandEncoder
#pragma push_macro("wgpuCommandEncoderBeginComputePass")
#undef wgpuCommandEncoderBeginComputePass
#pragma push_macro("wgpuCommandEncoderBeginRenderPass")
#undef wgpuCommandEncoderBeginRenderPass
#pragma push_macro("wgpuCommandEncoderClearBuffer")
#undef wgpuCommandEncoderClearBuffer
#pragma push_macro("wgpuCommandEncoderCopyBufferToBuffer")
#undef wgpuCommandEncoderCopyBufferToBuffer
#pragma push_macro("wgpuCommandEncoderCopyBufferToTexture")
#undef wgpuCommandEncoderCopyBufferToTexture
#pragma push_macro("wgpuCommandEncoderCopyTextureToBuffer")
#undef wgpuCommandEncoderCopyTextureToBuffer
#pragma push_macro("wgpuCommandEncoderCopyTextureToTexture")
#undef wgpuCommandEncoderCopyTextureToTexture
#pragma push_macro("wgpuCommandEncoderCopyTextureToTextureInternal")
#undef wgpuCommandEncoderCopyTextureToTextureInternal
#pragma push_macro("wgpuCommandEncoderFinish")
#undef wgpuCommandEncoderFinish
#pragma push_macro("wgpuCommandEncoderInjectValidationError")
#undef wgpuCommandEncoderInjectValidationError
#pragma push_macro("wgpuCommandEncoderInsertDebugMarker")
#undef wgpuCommandEncoderInsertDebugMarker
#pragma push_macro("wgpuCommandEncoderPopDebugGroup")
#undef wgpuCommandEncoderPopDebugGroup
#pragma push_macro("wgpuCommandEncoderPushDebugGroup")
#undef wgpuCommandEncoderPushDebugGroup
#pragma push_macro("wgpuCommandEncoderResolveQuerySet")
#undef wgpuCommandEncoderResolveQuerySet
#pragma push_macro("wgpuCommandEncoderSetLabel")
#undef wgpuCommandEncoderSetLabel
#pragma push_macro("wgpuCommandEncoderWriteBuffer")
#undef wgpuCommandEncoderWriteBuffer
#pragma push_macro("wgpuCommandEncoderWriteTimestamp")
#undef wgpuCommandEncoderWriteTimestamp
#pragma push_macro("wgpuCommandEncoderAddRef")
#undef wgpuCommandEncoderAddRef
#pragma push_macro("wgpuCommandEncoderRelease")
#undef wgpuCommandEncoderRelease

// Methods of ComputePassEncoder
#pragma push_macro("wgpuComputePassEncoderDispatchWorkgroups")
#undef wgpuComputePassEncoderDispatchWorkgroups
#pragma push_macro("wgpuComputePassEncoderDispatchWorkgroupsIndirect")
#undef wgpuComputePassEncoderDispatchWorkgroupsIndirect
#pragma push_macro("wgpuComputePassEncoderEnd")
#undef wgpuComputePassEncoderEnd
#pragma push_macro("wgpuComputePassEncoderInsertDebugMarker")
#undef wgpuComputePassEncoderInsertDebugMarker
#pragma push_macro("wgpuComputePassEncoderPopDebugGroup")
#undef wgpuComputePassEncoderPopDebugGroup
#pragma push_macro("wgpuComputePassEncoderPushDebugGroup")
#undef wgpuComputePassEncoderPushDebugGroup
#pragma push_macro("wgpuComputePassEncoderSetBindGroup")
#undef wgpuComputePassEncoderSetBindGroup
#pragma push_macro("wgpuComputePassEncoderSetLabel")
#undef wgpuComputePassEncoderSetLabel
#pragma push_macro("wgpuComputePassEncoderSetPipeline")
#undef wgpuComputePassEncoderSetPipeline
#pragma push_macro("wgpuComputePassEncoderWriteTimestamp")
#undef wgpuComputePassEncoderWriteTimestamp
#pragma push_macro("wgpuComputePassEncoderAddRef")
#undef wgpuComputePassEncoderAddRef
#pragma push_macro("wgpuComputePassEncoderRelease")
#undef wgpuComputePassEncoderRelease

// Methods of ComputePipeline
#pragma push_macro("wgpuComputePipelineGetBindGroupLayout")
#undef wgpuComputePipelineGetBindGroupLayout
#pragma push_macro("wgpuComputePipelineSetLabel")
#undef wgpuComputePipelineSetLabel
#pragma push_macro("wgpuComputePipelineAddRef")
#undef wgpuComputePipelineAddRef
#pragma push_macro("wgpuComputePipelineRelease")
#undef wgpuComputePipelineRelease

// Methods of Device
#pragma push_macro("wgpuDeviceCreateBindGroup")
#undef wgpuDeviceCreateBindGroup
#pragma push_macro("wgpuDeviceCreateBindGroupLayout")
#undef wgpuDeviceCreateBindGroupLayout
#pragma push_macro("wgpuDeviceCreateBuffer")
#undef wgpuDeviceCreateBuffer
#pragma push_macro("wgpuDeviceCreateCommandEncoder")
#undef wgpuDeviceCreateCommandEncoder
#pragma push_macro("wgpuDeviceCreateComputePipeline")
#undef wgpuDeviceCreateComputePipeline
#pragma push_macro("wgpuDeviceCreateComputePipelineAsync")
#undef wgpuDeviceCreateComputePipelineAsync
#pragma push_macro("wgpuDeviceCreateErrorBuffer")
#undef wgpuDeviceCreateErrorBuffer
#pragma push_macro("wgpuDeviceCreateErrorExternalTexture")
#undef wgpuDeviceCreateErrorExternalTexture
#pragma push_macro("wgpuDeviceCreateErrorTexture")
#undef wgpuDeviceCreateErrorTexture
#pragma push_macro("wgpuDeviceCreateExternalTexture")
#undef wgpuDeviceCreateExternalTexture
#pragma push_macro("wgpuDeviceCreatePipelineLayout")
#undef wgpuDeviceCreatePipelineLayout
#pragma push_macro("wgpuDeviceCreateQuerySet")
#undef wgpuDeviceCreateQuerySet
#pragma push_macro("wgpuDeviceCreateRenderBundleEncoder")
#undef wgpuDeviceCreateRenderBundleEncoder
#pragma push_macro("wgpuDeviceCreateRenderPipeline")
#undef wgpuDeviceCreateRenderPipeline
#pragma push_macro("wgpuDeviceCreateRenderPipelineAsync")
#undef wgpuDeviceCreateRenderPipelineAsync
#pragma push_macro("wgpuDeviceCreateSampler")
#undef wgpuDeviceCreateSampler
#pragma push_macro("wgpuDeviceCreateShaderModule")
#undef wgpuDeviceCreateShaderModule
#pragma push_macro("wgpuDeviceCreateTexture")
#undef wgpuDeviceCreateTexture
#pragma push_macro("wgpuDeviceDestroy")
#undef wgpuDeviceDestroy
#pragma push_macro("wgpuDeviceEnumerateFeatures")
#undef wgpuDeviceEnumerateFeatures
#pragma push_macro("wgpuDeviceForceLoss")
#undef wgpuDeviceForceLoss
#pragma push_macro("wgpuDeviceGetAdapter")
#undef wgpuDeviceGetAdapter
#pragma push_macro("wgpuDeviceGetLimits")
#undef wgpuDeviceGetLimits
#pragma push_macro("wgpuDeviceGetQueue")
#undef wgpuDeviceGetQueue
#pragma push_macro("wgpuDeviceGetSupportedSurfaceUsage")
#undef wgpuDeviceGetSupportedSurfaceUsage
#pragma push_macro("wgpuDeviceHasFeature")
#undef wgpuDeviceHasFeature
#pragma push_macro("wgpuDeviceInjectError")
#undef wgpuDeviceInjectError
#pragma push_macro("wgpuDevicePopErrorScope")
#undef wgpuDevicePopErrorScope
#pragma push_macro("wgpuDevicePushErrorScope")
#undef wgpuDevicePushErrorScope
#pragma push_macro("wgpuDeviceSetDeviceLostCallback")
#undef wgpuDeviceSetDeviceLostCallback
#pragma push_macro("wgpuDeviceSetLabel")
#undef wgpuDeviceSetLabel
#pragma push_macro("wgpuDeviceSetLoggingCallback")
#undef wgpuDeviceSetLoggingCallback
#pragma push_macro("wgpuDeviceSetUncapturedErrorCallback")
#undef wgpuDeviceSetUncapturedErrorCallback
#pragma push_macro("wgpuDeviceTick")
#undef wgpuDeviceTick
#pragma push_macro("wgpuDeviceValidateTextureDescriptor")
#undef wgpuDeviceValidateTextureDescriptor
#pragma push_macro("wgpuDeviceAddRef")
#undef wgpuDeviceAddRef
#pragma push_macro("wgpuDeviceRelease")
#undef wgpuDeviceRelease

// Methods of ExternalTexture
#pragma push_macro("wgpuExternalTextureDestroy")
#undef wgpuExternalTextureDestroy
#pragma push_macro("wgpuExternalTextureExpire")
#undef wgpuExternalTextureExpire
#pragma push_macro("wgpuExternalTextureRefresh")
#undef wgpuExternalTextureRefresh
#pragma push_macro("wgpuExternalTextureSetLabel")
#undef wgpuExternalTextureSetLabel
#pragma push_macro("wgpuExternalTextureAddRef")
#undef wgpuExternalTextureAddRef
#pragma push_macro("wgpuExternalTextureRelease")
#undef wgpuExternalTextureRelease

// Methods of Instance
#pragma push_macro("wgpuInstanceCreateSurface")
#undef wgpuInstanceCreateSurface
#pragma push_macro("wgpuInstanceProcessEvents")
#undef wgpuInstanceProcessEvents
#pragma push_macro("wgpuInstanceRequestAdapter")
#undef wgpuInstanceRequestAdapter
#pragma push_macro("wgpuInstanceAddRef")
#undef wgpuInstanceAddRef
#pragma push_macro("wgpuInstanceRelease")
#undef wgpuInstanceRelease

// Methods of PipelineLayout
#pragma push_macro("wgpuPipelineLayoutSetLabel")
#undef wgpuPipelineLayoutSetLabel
#pragma push_macro("wgpuPipelineLayoutAddRef")
#undef wgpuPipelineLayoutAddRef
#pragma push_macro("wgpuPipelineLayoutRelease")
#undef wgpuPipelineLayoutRelease

// Methods of QuerySet
#pragma push_macro("wgpuQuerySetDestroy")
#undef wgpuQuerySetDestroy
#pragma push_macro("wgpuQuerySetGetCount")
#undef wgpuQuerySetGetCount
#pragma push_macro("wgpuQuerySetGetType")
#undef wgpuQuerySetGetType
#pragma push_macro("wgpuQuerySetSetLabel")
#undef wgpuQuerySetSetLabel
#pragma push_macro("wgpuQuerySetAddRef")
#undef wgpuQuerySetAddRef
#pragma push_macro("wgpuQuerySetRelease")
#undef wgpuQuerySetRelease

// Methods of Queue
#pragma push_macro("wgpuQueueCopyExternalTextureForBrowser")
#undef wgpuQueueCopyExternalTextureForBrowser
#pragma push_macro("wgpuQueueCopyTextureForBrowser")
#undef wgpuQueueCopyTextureForBrowser
#pragma push_macro("wgpuQueueOnSubmittedWorkDone")
#undef wgpuQueueOnSubmittedWorkDone
#pragma push_macro("wgpuQueueSetLabel")
#undef wgpuQueueSetLabel
#pragma push_macro("wgpuQueueSubmit")
#undef wgpuQueueSubmit
#pragma push_macro("wgpuQueueWriteBuffer")
#undef wgpuQueueWriteBuffer
#pragma push_macro("wgpuQueueWriteBuffer2")
#undef wgpuQueueWriteBuffer2
#pragma push_macro("wgpuQueueWriteTexture")
#undef wgpuQueueWriteTexture
#pragma push_macro("wgpuQueueAddRef")
#undef wgpuQueueAddRef
#pragma push_macro("wgpuQueueRelease")
#undef wgpuQueueRelease

// Methods of RenderBundle
#pragma push_macro("wgpuRenderBundleAddRef")
#undef wgpuRenderBundleAddRef
#pragma push_macro("wgpuRenderBundleRelease")
#undef wgpuRenderBundleRelease

// Methods of RenderBundleEncoder
#pragma push_macro("wgpuRenderBundleEncoderDraw")
#undef wgpuRenderBundleEncoderDraw
#pragma push_macro("wgpuRenderBundleEncoderDrawIndexed")
#undef wgpuRenderBundleEncoderDrawIndexed
#pragma push_macro("wgpuRenderBundleEncoderDrawIndexedIndirect")
#undef wgpuRenderBundleEncoderDrawIndexedIndirect
#pragma push_macro("wgpuRenderBundleEncoderDrawIndirect")
#undef wgpuRenderBundleEncoderDrawIndirect
#pragma push_macro("wgpuRenderBundleEncoderFinish")
#undef wgpuRenderBundleEncoderFinish
#pragma push_macro("wgpuRenderBundleEncoderInsertDebugMarker")
#undef wgpuRenderBundleEncoderInsertDebugMarker
#pragma push_macro("wgpuRenderBundleEncoderPopDebugGroup")
#undef wgpuRenderBundleEncoderPopDebugGroup
#pragma push_macro("wgpuRenderBundleEncoderPushDebugGroup")
#undef wgpuRenderBundleEncoderPushDebugGroup
#pragma push_macro("wgpuRenderBundleEncoderSetBindGroup")
#undef wgpuRenderBundleEncoderSetBindGroup
#pragma push_macro("wgpuRenderBundleEncoderSetIndexBuffer")
#undef wgpuRenderBundleEncoderSetIndexBuffer
#pragma push_macro("wgpuRenderBundleEncoderSetLabel")
#undef wgpuRenderBundleEncoderSetLabel
#pragma push_macro("wgpuRenderBundleEncoderSetPipeline")
#undef wgpuRenderBundleEncoderSetPipeline
#pragma push_macro("wgpuRenderBundleEncoderSetVertexBuffer")
#undef wgpuRenderBundleEncoderSetVertexBuffer
#pragma push_macro("wgpuRenderBundleEncoderAddRef")
#undef wgpuRenderBundleEncoderAddRef
#pragma push_macro("wgpuRenderBundleEncoderRelease")
#undef wgpuRenderBundleEncoderRelease

// Methods of RenderPassEncoder
#pragma push_macro("wgpuRenderPassEncoderBeginOcclusionQuery")
#undef wgpuRenderPassEncoderBeginOcclusionQuery
#pragma push_macro("wgpuRenderPassEncoderDraw")
#undef wgpuRenderPassEncoderDraw
#pragma push_macro("wgpuRenderPassEncoderDrawIndexed")
#undef wgpuRenderPassEncoderDrawIndexed
#pragma push_macro("wgpuRenderPassEncoderDrawIndexedIndirect")
#undef wgpuRenderPassEncoderDrawIndexedIndirect
#pragma push_macro("wgpuRenderPassEncoderDrawIndirect")
#undef wgpuRenderPassEncoderDrawIndirect
#pragma push_macro("wgpuRenderPassEncoderEnd")
#undef wgpuRenderPassEncoderEnd
#pragma push_macro("wgpuRenderPassEncoderEndOcclusionQuery")
#undef wgpuRenderPassEncoderEndOcclusionQuery
#pragma push_macro("wgpuRenderPassEncoderExecuteBundles")
#undef wgpuRenderPassEncoderExecuteBundles
#pragma push_macro("wgpuRenderPassEncoderInsertDebugMarker")
#undef wgpuRenderPassEncoderInsertDebugMarker
#pragma push_macro("wgpuRenderPassEncoderPopDebugGroup")
#undef wgpuRenderPassEncoderPopDebugGroup
#pragma push_macro("wgpuRenderPassEncoderPushDebugGroup")
#undef wgpuRenderPassEncoderPushDebugGroup
#pragma push_macro("wgpuRenderPassEncoderSetBindGroup")
#undef wgpuRenderPassEncoderSetBindGroup
#pragma push_macro("wgpuRenderPassEncoderSetBlendConstant")
#undef wgpuRenderPassEncoderSetBlendConstant
#pragma push_macro("wgpuRenderPassEncoderSetIndexBuffer")
#undef wgpuRenderPassEncoderSetIndexBuffer
#pragma push_macro("wgpuRenderPassEncoderSetLabel")
#undef wgpuRenderPassEncoderSetLabel
#pragma push_macro("wgpuRenderPassEncoderSetPipeline")
#undef wgpuRenderPassEncoderSetPipeline
#pragma push_macro("wgpuRenderPassEncoderSetScissorRect")
#undef wgpuRenderPassEncoderSetScissorRect
#pragma push_macro("wgpuRenderPassEncoderSetStencilReference")
#undef wgpuRenderPassEncoderSetStencilReference
#pragma push_macro("wgpuRenderPassEncoderSetVertexBuffer")
#undef wgpuRenderPassEncoderSetVertexBuffer
#pragma push_macro("wgpuRenderPassEncoderSetViewport")
#undef wgpuRenderPassEncoderSetViewport
#pragma push_macro("wgpuRenderPassEncoderWriteTimestamp")
#undef wgpuRenderPassEncoderWriteTimestamp
#pragma push_macro("wgpuRenderPassEncoderAddRef")
#undef wgpuRenderPassEncoderAddRef
#pragma push_macro("wgpuRenderPassEncoderRelease")
#undef wgpuRenderPassEncoderRelease

// Methods of RenderPipeline
#pragma push_macro("wgpuRenderPipelineGetBindGroupLayout")
#undef wgpuRenderPipelineGetBindGroupLayout
#pragma push_macro("wgpuRenderPipelineSetLabel")
#undef wgpuRenderPipelineSetLabel
#pragma push_macro("wgpuRenderPipelineAddRef")
#undef wgpuRenderPipelineAddRef
#pragma push_macro("wgpuRenderPipelineRelease")
#undef wgpuRenderPipelineRelease

// Methods of Sampler
#pragma push_macro("wgpuSamplerSetLabel")
#undef wgpuSamplerSetLabel
#pragma push_macro("wgpuSamplerAddRef")
#undef wgpuSamplerAddRef
#pragma push_macro("wgpuSamplerRelease")
#undef wgpuSamplerRelease

// Methods of ShaderModule
#pragma push_macro("wgpuShaderModuleGetCompilationInfo")
#undef wgpuShaderModuleGetCompilationInfo
#pragma push_macro("wgpuShaderModuleSetLabel")
#undef wgpuShaderModuleSetLabel
#pragma push_macro("wgpuShaderModuleAddRef")
#undef wgpuShaderModuleAddRef
#pragma push_macro("wgpuShaderModuleRelease")
#undef wgpuShaderModuleRelease

// Methods of Surface
#pragma push_macro("wgpuSurfaceAddRef")
#undef wgpuSurfaceAddRef
#pragma push_macro("wgpuSurfaceRelease")
#undef wgpuSurfaceRelease
#pragma push_macro("wgpuSurfacePresent")
#undef wgpuSurfacePresent
#pragma push_macro("wgpuSurfaceGetCapabilities")
#undef wgpuSurfaceGetCapabilities
#pragma push_macro("wgpuSurfaceConfigure")
#undef wgpuSurfaceConfigure
#pragma push_macro("wgpuSurfaceGetCurrentTexture")
#undef wgpuSurfaceGetCurrentTexture
#pragma push_macro("wgpuSurfaceCapabilitiesFreeMembers")
#undef wgpuSurfaceCapabilitiesFreeMembers

// Methods of Texture
#pragma push_macro("wgpuTextureCreateView")
#undef wgpuTextureCreateView
#pragma push_macro("wgpuTextureDestroy")
#undef wgpuTextureDestroy
#pragma push_macro("wgpuTextureGetDepthOrArrayLayers")
#undef wgpuTextureGetDepthOrArrayLayers
#pragma push_macro("wgpuTextureGetDimension")
#undef wgpuTextureGetDimension
#pragma push_macro("wgpuTextureGetFormat")
#undef wgpuTextureGetFormat
#pragma push_macro("wgpuTextureGetHeight")
#undef wgpuTextureGetHeight
#pragma push_macro("wgpuTextureGetMipLevelCount")
#undef wgpuTextureGetMipLevelCount
#pragma push_macro("wgpuTextureGetSampleCount")
#undef wgpuTextureGetSampleCount
#pragma push_macro("wgpuTextureGetUsage")
#undef wgpuTextureGetUsage
#pragma push_macro("wgpuTextureGetWidth")
#undef wgpuTextureGetWidth
#pragma push_macro("wgpuTextureSetLabel")
#undef wgpuTextureSetLabel
#pragma push_macro("wgpuTextureAddRef")
#undef wgpuTextureAddRef
#pragma push_macro("wgpuTextureRelease")
#undef wgpuTextureRelease

// Methods of TextureView
#pragma push_macro("wgpuTextureViewSetLabel")
#undef wgpuTextureViewSetLabel
#pragma push_macro("wgpuTextureViewAddRef")
#undef wgpuTextureViewAddRef
#pragma push_macro("wgpuTextureViewRelease")
#undef wgpuTextureViewRelease
#endif
#if defined(POP_WEBGPU)
#undef POP_WEBGPU
#endif
#if defined(PUSH_WEBGPU)
#undef PUSH_WEBGPU
#endif
