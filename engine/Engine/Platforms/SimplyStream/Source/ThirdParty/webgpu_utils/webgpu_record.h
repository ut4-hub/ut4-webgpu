#pragma once
#ifndef WNDR_SKIP_WGPU_RECORD
namespace record {
	WNDR_FORCE_STATIC_INLINE auto wgpuCreateInstance(WGPUInstanceDescriptor const* descriptor) {
		return ::wgpuCreateInstance(descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuGetProcAddress(WGPUDevice device, char const* procName) {
		return ::wgpuGetProcAddress(device, procName);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterCreateDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor /* nullable */) {
		return ::wgpuAdapterCreateDevice(adapter, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterEnumerateFeatures(WGPUAdapter adapter, WGPUFeatureName* features) {
		return ::wgpuAdapterEnumerateFeatures(adapter, features);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterGetInstance(WGPUAdapter adapter) {
		return ::wgpuAdapterGetInstance(adapter);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterGetLimits(WGPUAdapter adapter, WGPULimits* limits) {
		return ::wgpuAdapterGetLimits(adapter, limits);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterGetInfo(WGPUAdapter adapter, WGPUAdapterInfo* properties) {
		return ::wgpuAdapterGetInfo(adapter, properties);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterHasFeature(WGPUAdapter adapter, WGPUFeatureName feature) {
		return ::wgpuAdapterHasFeature(adapter, feature);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterRequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor /* nullable */, WGPURequestDeviceCallback callback, void* userdata) {
		return ::wgpuAdapterRequestDevice(adapter, descriptor, callback, userdata);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterAddRef(WGPUAdapter adapter) {
		return ::wgpuAdapterAddRef(adapter);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuAdapterRelease(WGPUAdapter adapter) {
		return ::wgpuAdapterRelease(adapter);
	}

	// Methods of BindGroup
	WNDR_FORCE_STATIC_INLINE auto wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, char const* label) {
		return ::wgpuBindGroupSetLabel(bindGroup, label);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBindGroupAddRef(WGPUBindGroup bindGroup) {
		return ::wgpuBindGroupAddRef(bindGroup);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBindGroupRelease(WGPUBindGroup bindGroup) {
		return ::wgpuBindGroupRelease(bindGroup);
	}

	// Methods of BindGroupLayout
	WNDR_FORCE_STATIC_INLINE auto wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, char const* label) {
		return ::wgpuBindGroupLayoutSetLabel(bindGroupLayout, label);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBindGroupLayoutAddRef(WGPUBindGroupLayout bindGroupLayout) {
		return ::wgpuBindGroupLayoutAddRef(bindGroupLayout);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout) {
		return ::wgpuBindGroupLayoutRelease(bindGroupLayout);
	}

	// Methods of Buffer
	WNDR_FORCE_STATIC_INLINE auto wgpuBufferDestroy(WGPUBuffer buffer) {
		return ::wgpuBufferDestroy(buffer);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
		return ::wgpuBufferGetConstMappedRange(buffer, offset, size);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferGetMapState(WGPUBuffer buffer) {
		return ::wgpuBufferGetMapState(buffer);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferGetMappedRangeAndWrite(WGPUBuffer buffer, size_t offset, size_t size, void* data) {
		return ::wgpuBufferGetMappedRangeAndWrite(buffer, offset, size, data);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferGetMappedRangeAndFill(WGPUBuffer buffer, size_t offset, size_t size, uint32_t data) {
		return ::wgpuBufferGetMappedRangeAndFill(buffer, offset, size, data);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
		return ::wgpuBufferGetMappedRange(buffer, offset, size);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferGetSize(WGPUBuffer buffer) {
		return ::wgpuBufferGetSize(buffer);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferGetUsage(WGPUBuffer buffer) {
		return ::wgpuBufferGetUsage(buffer);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallback callback, void* userdata) {
		return ::wgpuBufferMapAsync(buffer, mode, offset, size, callback, userdata);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferSetLabel(WGPUBuffer buffer, char const* label) {
		return ::wgpuBufferSetLabel(buffer, label);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferUnmap(WGPUBuffer buffer) {
		return ::wgpuBufferUnmap(buffer);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferAddRef(WGPUBuffer buffer) {
		return ::wgpuBufferAddRef(buffer);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuBufferRelease(WGPUBuffer buffer) {
		return ::wgpuBufferRelease(buffer);
	}

	// Methods of CommandBuffer
	WNDR_FORCE_STATIC_INLINE auto wgpuCommandBufferSetLabel(WGPUCommandBuffer commandBuffer, char const* label) {
		return ::wgpuCommandBufferSetLabel(commandBuffer, label);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandBufferAddRef(WGPUCommandBuffer commandBuffer) {
		return ::wgpuCommandBufferAddRef(commandBuffer);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer) {
		return ::wgpuCommandBufferRelease(commandBuffer);
	}

	// Methods of CommandEncoder
	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, WGPUComputePassDescriptor const* descriptor /* nullable */) {
		return ::wgpuCommandEncoderBeginComputePass(commandEncoder, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, WGPURenderPassDescriptor const* descriptor) {
		return ::wgpuCommandEncoderBeginRenderPass(commandEncoder, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderClearBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
		return ::wgpuCommandEncoderClearBuffer(commandEncoder, buffer, offset, size);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size) {
		return ::wgpuCommandEncoderCopyBufferToBuffer(commandEncoder, source, sourceOffset, destination, destinationOffset, size);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder commandEncoder, WGPUImageCopyBuffer const* source, WGPUImageCopyTexture const* destination, WGPUExtent3D const* copySize) {
		return ::wgpuCommandEncoderCopyBufferToTexture(commandEncoder, source, destination, copySize);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder commandEncoder, WGPUImageCopyTexture const* source, WGPUImageCopyBuffer const* destination, WGPUExtent3D const* copySize) {
		return ::wgpuCommandEncoderCopyTextureToBuffer(commandEncoder, source, destination, copySize);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder commandEncoder, WGPUImageCopyTexture const* source, WGPUImageCopyTexture const* destination, WGPUExtent3D const* copySize) {
		return ::wgpuCommandEncoderCopyTextureToTexture(commandEncoder, source, destination, copySize);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, WGPUCommandBufferDescriptor const* descriptor /* nullable */) {
		return ::wgpuCommandEncoderFinish(commandEncoder, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderInjectValidationError(WGPUCommandEncoder commandEncoder, char const* message) {
		return ::wgpuCommandEncoderInjectValidationError(commandEncoder, message);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, char const* markerLabel) {
		return ::wgpuCommandEncoderInsertDebugMarker(commandEncoder, markerLabel);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder) {
		return ::wgpuCommandEncoderPopDebugGroup(commandEncoder);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, char const* groupLabel) {
		return ::wgpuCommandEncoderPushDebugGroup(commandEncoder, groupLabel);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset) {
		return ::wgpuCommandEncoderResolveQuerySet(commandEncoder, querySet, firstQuery, queryCount, destination, destinationOffset);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, char const* label) {
		return ::wgpuCommandEncoderSetLabel(commandEncoder, label);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderWriteBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t bufferOffset, uint8_t const* data, uint64_t size) {
		return ::wgpuCommandEncoderWriteBuffer(commandEncoder, buffer, bufferOffset, data, size);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex) {
		return ::wgpuCommandEncoderWriteTimestamp(commandEncoder, querySet, queryIndex);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderAddRef(WGPUCommandEncoder commandEncoder) {
		return ::wgpuCommandEncoderAddRef(commandEncoder);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder) {
		return ::wgpuCommandEncoderRelease(commandEncoder);
	}

	// Methods of ComputePassEncoder
	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder computePassEncoder, uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ) {
		return ::wgpuComputePassEncoderDispatchWorkgroups(computePassEncoder, workgroupCountX, workgroupCountY, workgroupCountZ);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderDispatchWorkgroupsIndirect(WGPUComputePassEncoder computePassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
		return ::wgpuComputePassEncoderDispatchWorkgroupsIndirect(computePassEncoder, indirectBuffer, indirectOffset);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderEnd(WGPUComputePassEncoder computePassEncoder) {
		return ::wgpuComputePassEncoderEnd(computePassEncoder);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder computePassEncoder, char const* markerLabel) {
		return ::wgpuComputePassEncoderInsertDebugMarker(computePassEncoder, markerLabel);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderPopDebugGroup(WGPUComputePassEncoder computePassEncoder) {
		return ::wgpuComputePassEncoderPopDebugGroup(computePassEncoder);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderPushDebugGroup(WGPUComputePassEncoder computePassEncoder, char const* groupLabel) {
		return ::wgpuComputePassEncoderPushDebugGroup(computePassEncoder, groupLabel);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder computePassEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, uint32_t const* dynamicOffsets) {
		return ::wgpuComputePassEncoderSetBindGroup(computePassEncoder, groupIndex, group, dynamicOffsetCount, dynamicOffsets);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, char const* label) {
		return ::wgpuComputePassEncoderSetLabel(computePassEncoder, label);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuInstanceProcessEvents(WGPUInstance instance) {
		return ::wgpuInstanceProcessEvents(instance);
	}
#if 0
	WNDR_FORCE_STATIC_INLINE auto wgpuFlush() {
		return ::wgpuFlush();
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuFlushAndWait() {
		return ::wgpuFlushAndWait();
	}
#endif
	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder computePassEncoder, WGPUComputePipeline pipeline) {
		return ::wgpuComputePassEncoderSetPipeline(computePassEncoder, pipeline);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderWriteTimestamp(WGPUComputePassEncoder computePassEncoder, WGPUQuerySet querySet, uint32_t queryIndex) {
		return ::wgpuComputePassEncoderWriteTimestamp(computePassEncoder, querySet, queryIndex);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderAddRef(WGPUComputePassEncoder computePassEncoder) {
		return ::wgpuComputePassEncoderAddRef(computePassEncoder);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePassEncoderRelease(WGPUComputePassEncoder computePassEncoder) {
		return ::wgpuComputePassEncoderRelease(computePassEncoder);
	}

	// Methods of ComputePipeline
	WNDR_FORCE_STATIC_INLINE auto wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline computePipeline, uint32_t groupIndex) {
		return ::wgpuComputePipelineGetBindGroupLayout(computePipeline, groupIndex);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePipelineSetLabel(WGPUComputePipeline computePipeline, char const* label) {
		return ::wgpuComputePipelineSetLabel(computePipeline, label);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePipelineAddRef(WGPUComputePipeline computePipeline) {
		return ::wgpuComputePipelineAddRef(computePipeline);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuComputePipelineRelease(WGPUComputePipeline computePipeline) {
		return ::wgpuComputePipelineRelease(computePipeline);
	}

	// Methods of Device
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateBindGroup(WGPUDevice device, WGPUBindGroupDescriptor const* descriptor) {
		return ::wgpuDeviceCreateBindGroup(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateBindGroupLayout(WGPUDevice device, WGPUBindGroupLayoutDescriptor const* descriptor) {
		return ::wgpuDeviceCreateBindGroupLayout(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateBuffer(WGPUDevice device, WGPUBufferDescriptor const* descriptor) {
		return ::wgpuDeviceCreateBuffer(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateCommandEncoder(WGPUDevice device, WGPUCommandEncoderDescriptor const* descriptor /* nullable */) {
		return ::wgpuDeviceCreateCommandEncoder(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateComputePipeline(WGPUDevice device, WGPUComputePipelineDescriptor const* descriptor) {
		return ::wgpuDeviceCreateComputePipeline(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, WGPUComputePipelineDescriptor const* descriptor, WGPUCreateComputePipelineAsyncCallback callback, void* userdata) {
		return ::wgpuDeviceCreateComputePipelineAsync(device, descriptor, callback, userdata);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateErrorBuffer(WGPUDevice device, WGPUBufferDescriptor const* descriptor) {
		return ::wgpuDeviceCreateErrorBuffer(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreatePipelineLayout(WGPUDevice device, WGPUPipelineLayoutDescriptor const* descriptor) {
		return ::wgpuDeviceCreatePipelineLayout(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateQuerySet(WGPUDevice device, WGPUQuerySetDescriptor const* descriptor) {
		return ::wgpuDeviceCreateQuerySet(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, WGPURenderBundleEncoderDescriptor const* descriptor) {
		return ::wgpuDeviceCreateRenderBundleEncoder(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateRenderPipeline(WGPUDevice device, WGPURenderPipelineDescriptor const* descriptor) {
		return ::wgpuDeviceCreateRenderPipeline(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, WGPURenderPipelineDescriptor const* descriptor, WGPUCreateRenderPipelineAsyncCallback callback, void* userdata) {
		return ::wgpuDeviceCreateRenderPipelineAsync(device, descriptor, callback, userdata);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateSampler(WGPUDevice device, WGPUSamplerDescriptor const* descriptor /* nullable */) {
		return ::wgpuDeviceCreateSampler(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateShaderModule(WGPUDevice device, WGPUShaderModuleDescriptor const* descriptor) {
		return ::wgpuDeviceCreateShaderModule(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceCreateTexture(WGPUDevice device, WGPUTextureDescriptor const* descriptor) {
		return ::wgpuDeviceCreateTexture(device, descriptor);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceDestroy(WGPUDevice device) {
		return ::wgpuDeviceDestroy(device);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceEnumerateFeatures(WGPUDevice device, WGPUFeatureName* features) {
		return ::wgpuDeviceEnumerateFeatures(device, features);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceForceLoss(WGPUDevice device, WGPUDeviceLostReason type, char const* message) {
		return ::wgpuDeviceForceLoss(device, type, message);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceGetAdapter(WGPUDevice device) {
		return ::wgpuDeviceGetAdapter(device);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceGetLimits(WGPUDevice device, WGPULimits* limits) {
		return ::wgpuDeviceGetLimits(device, limits);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceGetQueue(WGPUDevice device) {
		return ::wgpuDeviceGetQueue(device);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceGetSupportedSurfaceUsage(WGPUDevice device, WGPUSurface surface) {
		return ::wgpuDeviceGetSupportedSurfaceUsage(device, surface);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature) {
		return ::wgpuDeviceHasFeature(device, feature);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceInjectError(WGPUDevice device, WGPUErrorType type, char const* message) {
		return ::wgpuDeviceInjectError(device, type, message);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDevicePopErrorScope(WGPUDevice device, WGPUErrorCallback callback, void* userdata) {
		return ::wgpuDevicePopErrorScope(device, callback, userdata);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter) {
		return ::wgpuDevicePushErrorScope(device, filter);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceSetDeviceLostCallback(WGPUDevice device, WGPUDeviceLostCallback callback, void* userdata) {
		return ::wgpuDeviceSetDeviceLostCallback(device, callback, userdata);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceSetLabel(WGPUDevice device, char const* label) {
		return ::wgpuDeviceSetLabel(device, label);
	}
#if !PLATFORM_WASM
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceSetLoggingCallback(WGPUDevice device, WGPULoggingCallback callback, void* userdata) {
		return ::wgpuDeviceSetLoggingCallback(device, callback, userdata);
	}
#endif
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceSetUncapturedErrorCallback(WGPUDevice device, WGPUErrorCallback callback, void* userdata) {
		return ::wgpuDeviceSetUncapturedErrorCallback(device, callback, userdata);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceTick(WGPUDevice device) {
		return ::wgpuDeviceTick(device);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceValidateTextureDescriptor(WGPUDevice device, WGPUTextureDescriptor const* descriptor) {
		return ::wgpuDeviceValidateTextureDescriptor(device, descriptor);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceAddRef(WGPUDevice device) {
		return ::wgpuDeviceAddRef(device);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuDeviceRelease(WGPUDevice device) {
		return ::wgpuDeviceRelease(device);
	}
	// Methods of Instance
	WNDR_FORCE_STATIC_INLINE auto wgpuInstanceCreateSurface(WGPUInstance instance, WGPUSurfaceDescriptor const* descriptor) {
		return ::wgpuInstanceCreateSurface(instance, descriptor);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuInstanceRequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const* options /* nullable */, WGPURequestAdapterCallback callback, void* userdata) {
		return ::wgpuInstanceRequestAdapter(instance, options, callback, userdata);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuInstanceAddRef(WGPUInstance instance) {
		return ::wgpuInstanceAddRef(instance);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuInstanceRelease(WGPUInstance instance) {
		return ::wgpuInstanceRelease(instance);
	}


	// Methods of PipelineLayout
	WNDR_FORCE_STATIC_INLINE auto wgpuPipelineLayoutSetLabel(WGPUPipelineLayout pipelineLayout, char const* label) {
		return ::wgpuPipelineLayoutSetLabel(pipelineLayout, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuPipelineLayoutAddRef(WGPUPipelineLayout pipelineLayout) {
		return ::wgpuPipelineLayoutAddRef(pipelineLayout);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuPipelineLayoutRelease(WGPUPipelineLayout pipelineLayout) {
		return ::wgpuPipelineLayoutRelease(pipelineLayout);
	}

	// Methods of QuerySet
	WNDR_FORCE_STATIC_INLINE auto wgpuQuerySetDestroy(WGPUQuerySet querySet) {
		return ::wgpuQuerySetDestroy(querySet);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQuerySetGetCount(WGPUQuerySet querySet) {
		return ::wgpuQuerySetGetCount(querySet);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQuerySetGetType(WGPUQuerySet querySet) {
		return ::wgpuQuerySetGetType(querySet);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQuerySetSetLabel(WGPUQuerySet querySet, char const* label) {
		return ::wgpuQuerySetSetLabel(querySet, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQuerySetAddRef(WGPUQuerySet querySet) {
		return ::wgpuQuerySetAddRef(querySet);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQuerySetRelease(WGPUQuerySet querySet) {
		return ::wgpuQuerySetRelease(querySet);
	}

	// Methods of Queue
	WNDR_FORCE_STATIC_INLINE auto wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, WGPUQueueWorkDoneCallback callback, void* userdata) {
		return ::wgpuQueueOnSubmittedWorkDone(queue, callback, userdata);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuQueueSetLabel(WGPUQueue queue, char const* label) {
		return ::wgpuQueueSetLabel(queue, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQueueSubmit(WGPUQueue queue, uint32_t commandCount, WGPUCommandBuffer const* commands) {
		return ::wgpuQueueSubmit(queue, commandCount, commands);
	}

	WNDR_FORCE_STATIC_INLINE auto wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, void const* data, size_t size) {
		return ::wgpuQueueWriteBuffer(queue, buffer, bufferOffset, data, size);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQueueWriteTexture(WGPUQueue queue, WGPUImageCopyTexture const* destination, void const* data, size_t dataSize, WGPUTextureDataLayout const* dataLayout, WGPUExtent3D const* writeSize) {
		return ::wgpuQueueWriteTexture(queue, destination, data, dataSize, dataLayout, writeSize);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQueueAddRef(WGPUQueue queue) {
		return ::wgpuQueueAddRef(queue);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuQueueRelease(WGPUQueue queue) {
		return ::wgpuQueueRelease(queue);
	}

	// Methods of RenderBundle
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleAddRef(WGPURenderBundle renderBundle) {
		return ::wgpuRenderBundleAddRef(renderBundle);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleRelease(WGPURenderBundle renderBundle) {
		return ::wgpuRenderBundleRelease(renderBundle);
	}

	// Methods of RenderBundleEncoder
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
		return ::wgpuRenderBundleEncoderDraw(renderBundleEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
		return ::wgpuRenderBundleEncoderDrawIndexed(renderBundleEncoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
		return ::wgpuRenderBundleEncoderDrawIndexedIndirect(renderBundleEncoder, indirectBuffer, indirectOffset);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
		return ::wgpuRenderBundleEncoderDrawIndirect(renderBundleEncoder, indirectBuffer, indirectOffset);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderBundleDescriptor const* descriptor /* nullable */) {
		return ::wgpuRenderBundleEncoderFinish(renderBundleEncoder, descriptor);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, char const* markerLabel) {
		return ::wgpuRenderBundleEncoderInsertDebugMarker(renderBundleEncoder, markerLabel);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder) {
		return ::wgpuRenderBundleEncoderPopDebugGroup(renderBundleEncoder);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, char const* groupLabel) {
		return ::wgpuRenderBundleEncoderPushDebugGroup(renderBundleEncoder, groupLabel);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, uint32_t const* dynamicOffsets) {
		return ::wgpuRenderBundleEncoderSetBindGroup(renderBundleEncoder, groupIndex, group, dynamicOffsetCount, dynamicOffsets);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
		return ::wgpuRenderBundleEncoderSetIndexBuffer(renderBundleEncoder, buffer, format, offset, size);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, char const* label) {
		return ::wgpuRenderBundleEncoderSetLabel(renderBundleEncoder, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline) {
		return ::wgpuRenderBundleEncoderSetPipeline(renderBundleEncoder, pipeline);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPUBuffer buffer /* nullable */, uint64_t offset, uint64_t size) {
		return ::wgpuRenderBundleEncoderSetVertexBuffer(renderBundleEncoder, slot, buffer, offset, size);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderAddRef(WGPURenderBundleEncoder renderBundleEncoder) {
		return ::wgpuRenderBundleEncoderAddRef(renderBundleEncoder);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder) {
		return ::wgpuRenderBundleEncoderRelease(renderBundleEncoder);
	}

	// Methods of RenderPassEncoder
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex) {
		return ::wgpuRenderPassEncoderBeginOcclusionQuery(renderPassEncoder, queryIndex);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
		return ::wgpuRenderPassEncoderDraw(renderPassEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
		return ::wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
		return ::wgpuRenderPassEncoderDrawIndexedIndirect(renderPassEncoder, indirectBuffer, indirectOffset);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
		return ::wgpuRenderPassEncoderDrawIndirect(renderPassEncoder, indirectBuffer, indirectOffset);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder) {
		return ::wgpuRenderPassEncoderEnd(renderPassEncoder);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder) {
		return ::wgpuRenderPassEncoderEndOcclusionQuery(renderPassEncoder);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, uint32_t bundleCount, WGPURenderBundle const* bundles) {
		return ::wgpuRenderPassEncoderExecuteBundles(renderPassEncoder, bundleCount, bundles);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, char const* markerLabel) {
		return ::wgpuRenderPassEncoderInsertDebugMarker(renderPassEncoder, markerLabel);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder) {
		return ::wgpuRenderPassEncoderPopDebugGroup(renderPassEncoder);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, char const* groupLabel) {
		return ::wgpuRenderPassEncoderPushDebugGroup(renderPassEncoder, groupLabel);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, uint32_t const* dynamicOffsets) {
		return ::wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, groupIndex, group, dynamicOffsetCount, dynamicOffsets);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder renderPassEncoder, WGPUColor const* color) {
		return ::wgpuRenderPassEncoderSetBlendConstant(renderPassEncoder, color);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
		return ::wgpuRenderPassEncoderSetIndexBuffer(renderPassEncoder, buffer, format, offset, size);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, char const* label) {
		return ::wgpuRenderPassEncoderSetLabel(renderPassEncoder, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline) {
		return ::wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pipeline);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
		return ::wgpuRenderPassEncoderSetScissorRect(renderPassEncoder, x, y, width, height);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference) {
		return ::wgpuRenderPassEncoderSetStencilReference(renderPassEncoder, reference);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder, uint32_t slot, WGPUBuffer buffer /* nullable */, uint64_t offset, uint64_t size) {
		return ::wgpuRenderPassEncoderSetVertexBuffer(renderPassEncoder, slot, buffer, offset, size);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth) {
		return ::wgpuRenderPassEncoderSetViewport(renderPassEncoder, x, y, width, height, minDepth, maxDepth);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderWriteTimestamp(WGPURenderPassEncoder renderPassEncoder, WGPUQuerySet querySet, uint32_t queryIndex) {
		return ::wgpuRenderPassEncoderWriteTimestamp(renderPassEncoder, querySet, queryIndex);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderAddRef(WGPURenderPassEncoder renderPassEncoder) {
		return ::wgpuRenderPassEncoderAddRef(renderPassEncoder);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder) {
		return ::wgpuRenderPassEncoderRelease(renderPassEncoder);
	}

	// Methods of RenderPipeline
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPipelineGetBindGroupLayout(WGPURenderPipeline renderPipeline, uint32_t groupIndex) {
		return ::wgpuRenderPipelineGetBindGroupLayout(renderPipeline, groupIndex);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPipelineSetLabel(WGPURenderPipeline renderPipeline, char const* label) {
		return ::wgpuRenderPipelineSetLabel(renderPipeline, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPipelineAddRef(WGPURenderPipeline renderPipeline) {
		return ::wgpuRenderPipelineAddRef(renderPipeline);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuRenderPipelineRelease(WGPURenderPipeline renderPipeline) {
		return ::wgpuRenderPipelineRelease(renderPipeline);
	}

	// Methods of Sampler
	WNDR_FORCE_STATIC_INLINE auto wgpuSamplerSetLabel(WGPUSampler sampler, char const* label) {
		return ::wgpuSamplerSetLabel(sampler, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuSamplerAddRef(WGPUSampler sampler) {
		return ::wgpuSamplerAddRef(sampler);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuSamplerRelease(WGPUSampler sampler) {
		return ::wgpuSamplerRelease(sampler);
	}

	// Methods of ShaderModule
	WNDR_FORCE_STATIC_INLINE auto wgpuShaderModuleGetCompilationInfo(WGPUShaderModule shaderModule, WGPUCompilationInfoCallback callback, void* userdata) {
		return ::wgpuShaderModuleGetCompilationInfo(shaderModule, callback, userdata);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuShaderModuleSetLabel(WGPUShaderModule shaderModule, char const* label) {
		return ::wgpuShaderModuleSetLabel(shaderModule, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuShaderModuleAddRef(WGPUShaderModule shaderModule) {
		return ::wgpuShaderModuleAddRef(shaderModule);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuShaderModuleRelease(WGPUShaderModule shaderModule) {
		return ::wgpuShaderModuleRelease(shaderModule);
	}

	// Methods of Surface
	WNDR_FORCE_STATIC_INLINE auto wgpuSurfaceAddRef(WGPUSurface surface) {
		return ::wgpuSurfaceAddRef(surface);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuSurfaceRelease(WGPUSurface surface) {
		return ::wgpuSurfaceRelease(surface);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuSurfacePresent(WGPUSurface surface) {
		return ::wgpuSurfacePresent(surface);
	}
	//WNDR_FORCE_STATIC_INLINE auto wgpuSurfaceGetCapabilities(WGPUSurface surface, WGPUAdapter adapter, WGPUSurfaceCapabilities* capabilities) {
	//	return ::wgpuSurfaceGetCapabilities(surface, adapter, capabilities);
	//}
	WNDR_FORCE_STATIC_INLINE auto wgpuSurfaceConfigure(WGPUSurface surface, WGPUSurfaceConfiguration const* config) {
		return ::wgpuSurfaceConfigure(surface, config);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuSurfaceGetCurrentTexture(WGPUSurface surface, WGPUSurfaceTexture* surfaceTexture) {
		return ::wgpuSurfaceGetCurrentTexture(surface, surfaceTexture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities value) {
		return ::wgpuSurfaceCapabilitiesFreeMembers(value);
	}

	// Methods of Texture
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureCreateView(WGPUTexture texture, WGPUTextureViewDescriptor const* descriptor /* nullable */) {
		return ::wgpuTextureCreateView(texture, descriptor);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureDestroy(WGPUTexture texture) {
		return ::wgpuTextureDestroy(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureGetDepthOrArrayLayers(WGPUTexture texture) {
		return ::wgpuTextureGetDepthOrArrayLayers(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureGetDimension(WGPUTexture texture) {
		return ::wgpuTextureGetDimension(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureGetFormat(WGPUTexture texture) {
		return ::wgpuTextureGetFormat(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureGetHeight(WGPUTexture texture) {
		return ::wgpuTextureGetHeight(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureGetMipLevelCount(WGPUTexture texture) {
		return ::wgpuTextureGetMipLevelCount(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureGetSampleCount(WGPUTexture texture) {
		return ::wgpuTextureGetSampleCount(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureGetUsage(WGPUTexture texture) {
		return ::wgpuTextureGetUsage(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureGetWidth(WGPUTexture texture) {
		return ::wgpuTextureGetWidth(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureSetLabel(WGPUTexture texture, char const* label) {
		return ::wgpuTextureSetLabel(texture, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureAddRef(WGPUTexture texture) {
		return ::wgpuTextureAddRef(texture);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureRelease(WGPUTexture texture) {
		return ::wgpuTextureRelease(texture);
	}

	// Methods of TextureView
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureViewSetLabel(WGPUTextureView textureView, char const* label) {
		return ::wgpuTextureViewSetLabel(textureView, label);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureViewAddRef(WGPUTextureView textureView) {
		return ::wgpuTextureViewAddRef(textureView);
	}
	WNDR_FORCE_STATIC_INLINE auto wgpuTextureViewRelease(WGPUTextureView textureView) {
		return ::wgpuTextureViewRelease(textureView);
	}
}
#endif
