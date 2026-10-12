
WireResult HandleAdapterRequestDeviceCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleBufferMapAsyncCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleDeviceCreateComputePipelineAsyncCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleDeviceCreateRenderPipelineAsyncCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleDeviceLoggingCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleDeviceLostCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleDevicePopErrorScopeCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleDeviceUncapturedErrorCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleInstanceRequestAdapterCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleQueueWorkDoneCallback(DeserializeBuffer* deserializeBuffer);
WireResult HandleShaderModuleGetCompilationInfoCallback(DeserializeBuffer* deserializeBuffer);

WireResult DoAdapterRequestDeviceCallback(ObjectHandle eventManager, WGPUFuture future, WGPURequestDeviceStatus status, WGPUStringView message, WGPULimits const * limits, uint32_t featuresCount, WGPUFeatureName const * features);
WireResult DoBufferMapAsyncCallback(ObjectHandle eventManager, WGPUFuture future, WGPUMapAsyncStatus status, WGPUStringView message, uint64_t readDataUpdateInfoLength, uint8_t const * readDataUpdateInfo);
WireResult DoDeviceCreateComputePipelineAsyncCallback(ObjectHandle eventManager, WGPUFuture future, WGPUCreatePipelineAsyncStatus status, WGPUStringView message);
WireResult DoDeviceCreateRenderPipelineAsyncCallback(ObjectHandle eventManager, WGPUFuture future, WGPUCreatePipelineAsyncStatus status, WGPUStringView message);
WireResult DoDeviceLoggingCallback(Device* device, WGPULoggingType type, WGPUStringView message);
WireResult DoDeviceLostCallback(ObjectHandle eventManager, WGPUFuture future, WGPUDeviceLostReason reason, WGPUStringView message);
WireResult DoDevicePopErrorScopeCallback(ObjectHandle eventManager, WGPUFuture future, WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message);
WireResult DoDeviceUncapturedErrorCallback(Device* device, WGPUErrorType type, WGPUStringView message);
WireResult DoInstanceRequestAdapterCallback(ObjectHandle eventManager, WGPUFuture future, WGPURequestAdapterStatus status, WGPUStringView message, WGPUAdapterInfo const * info, WGPULimits const * limits, uint32_t featuresCount, WGPUFeatureName const * features);
WireResult DoQueueWorkDoneCallback(ObjectHandle eventManager, WGPUFuture future, WGPUQueueWorkDoneStatus status, WGPUStringView message);
WireResult DoShaderModuleGetCompilationInfoCallback(ObjectHandle eventManager, WGPUFuture future, WGPUCompilationInfoRequestStatus status, WGPUCompilationInfo const * info);
