
#include <algorithm>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <vector>
#include <utility>

#include "dawn/wire/client/Client.h"
#include "dawn/wire/client/webgpu.h"

namespace dawn::wire::client {

    // Template function for constexpr branching when creating new objects.
    template <typename Parent, typename Child, typename... Args>
    Child* Create(Parent p, Args... args) {
        if constexpr (std::is_constructible_v<Child, const ObjectBaseParams&, decltype(args)...>) {
            return p->GetClient()->template Make<Child>(args...).Detach();
        } else if constexpr (std::is_constructible_v<Child, const ObjectBaseParams&, const ObjectHandle&, decltype(args)...>) {
            return p->GetClient()->template Make<Child>(p->GetEventManagerHandle(), args...).Detach();
        } else {
            if constexpr (std::is_base_of_v<ObjectWithEventsBase, Child>) {
                return p->GetClient()->template Make<Child>(p->GetEventManagerHandle()).Detach();
            } else {
                return p->GetClient()->template Make<Child>().Detach();
            }
        }
    }

}  // namespace dawn::wire::client


DAWN_WIRE_EXPORT WGPUDevice wgpuDawnWireClientAdapterCreateDevice(WGPUAdapter cSelf, WGPUDeviceDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APICreateDevice( descriptor);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientAdapterGetFeatures(WGPUAdapter cSelf, WGPUSupportedFeatures * features) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIGetFeatures( features);
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientAdapterGetFormatCapabilities(WGPUAdapter cSelf, WGPUTextureFormat format, WGPUDawnFormatCapabilities * capabilities) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIGetFormatCapabilities( format,  capabilities);
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientAdapterGetInfo(WGPUAdapter cSelf, WGPUAdapterInfo * info) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIGetInfo( info);
}

DAWN_WIRE_EXPORT WGPUInstance wgpuDawnWireClientAdapterGetInstance(WGPUAdapter cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIGetInstance();
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientAdapterGetLimits(WGPUAdapter cSelf, WGPULimits * limits) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIGetLimits( limits);
}

DAWN_WIRE_EXPORT WGPUBool wgpuDawnWireClientAdapterHasFeature(WGPUAdapter cSelf, WGPUFeatureName feature) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIHasFeature( feature);
}

DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientAdapterRequestDevice(WGPUAdapter cSelf, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIRequestDevice( descriptor,  callbackInfo);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientAdapterAddRef(WGPUAdapter cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientAdapterRelease(WGPUAdapter cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Adapter*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientAdapterInfoFreeMembers(WGPUAdapterInfo cSelf) {
    return dawn::wire::client::APIFreeMembers(cSelf);
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientAdapterPropertiesMemoryHeapsFreeMembers(WGPUAdapterPropertiesMemoryHeaps cSelf) {
    return dawn::wire::client::APIFreeMembers(cSelf);
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientAdapterPropertiesSubgroupMatrixConfigsFreeMembers(WGPUAdapterPropertiesSubgroupMatrixConfigs cSelf) {
    return dawn::wire::client::APIFreeMembers(cSelf);
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientBindGroupDestroy(WGPUBindGroup cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::BindGroup*>(cSelf);
    dawn::wire::BindGroupDestroyCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBindGroupSetLabel(WGPUBindGroup cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::BindGroup*>(cSelf);
    dawn::wire::BindGroupSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBindGroupAddRef(WGPUBindGroup cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::BindGroup*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBindGroupRelease(WGPUBindGroup cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::BindGroup*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientBindGroupLayoutSetLabel(WGPUBindGroupLayout cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::BindGroupLayout*>(cSelf);
    dawn::wire::BindGroupLayoutSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBindGroupLayoutAddRef(WGPUBindGroupLayout cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::BindGroupLayout*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBindGroupLayoutRelease(WGPUBindGroupLayout cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::BindGroupLayout*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT WGPUTexelBufferView wgpuDawnWireClientBufferCreateTexelView(WGPUBuffer cSelf, WGPUTexelBufferViewDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    dawn::wire::BufferCreateTexelViewCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::TexelBufferView* returnObject = dawn::wire::client::Create<dawn::wire::client::Buffer*, dawn::wire::client::TexelBufferView>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBufferDestroy(WGPUBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIDestroy();
}

DAWN_WIRE_EXPORT void const * wgpuDawnWireClientBufferGetConstMappedRange(WGPUBuffer cSelf, size_t offset, size_t size) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIGetConstMappedRange( offset,  size);
}

DAWN_WIRE_EXPORT void * wgpuDawnWireClientBufferGetMappedRange(WGPUBuffer cSelf, size_t offset, size_t size) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIGetMappedRange( offset,  size);
}

DAWN_WIRE_EXPORT WGPUBufferMapState wgpuDawnWireClientBufferGetMapState(WGPUBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIGetMapState();
}

DAWN_WIRE_EXPORT uint64_t wgpuDawnWireClientBufferGetSize(WGPUBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIGetSize();
}

DAWN_WIRE_EXPORT WGPUBufferUsage wgpuDawnWireClientBufferGetUsage(WGPUBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIGetUsage();
}

DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientBufferMapAsync(WGPUBuffer cSelf, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIMapAsync( mode,  offset,  size,  callbackInfo);
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientBufferReadMappedRange(WGPUBuffer cSelf, size_t offset, void * data, size_t size) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIReadMappedRange( offset,  data,  size);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBufferSetLabel(WGPUBuffer cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    dawn::wire::BufferSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBufferUnmap(WGPUBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIUnmap();
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientBufferWriteMappedRange(WGPUBuffer cSelf, size_t offset, void const * data, size_t size) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIWriteMappedRange( offset,  data,  size);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBufferAddRef(WGPUBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientBufferRelease(WGPUBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Buffer*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandBufferSetLabel(WGPUCommandBuffer cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::CommandBuffer*>(cSelf);
    dawn::wire::CommandBufferSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandBufferAddRef(WGPUCommandBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::CommandBuffer*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandBufferRelease(WGPUCommandBuffer cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::CommandBuffer*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT WGPUComputePassEncoder wgpuDawnWireClientCommandEncoderBeginComputePass(WGPUCommandEncoder cSelf, WGPUComputePassDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderBeginComputePassCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::ComputePassEncoder* returnObject = dawn::wire::client::Create<dawn::wire::client::CommandEncoder*, dawn::wire::client::ComputePassEncoder>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPURenderPassEncoder wgpuDawnWireClientCommandEncoderBeginRenderPass(WGPUCommandEncoder cSelf, WGPURenderPassDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderBeginRenderPassCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::RenderPassEncoder* returnObject = dawn::wire::client::Create<dawn::wire::client::CommandEncoder*, dawn::wire::client::RenderPassEncoder>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderClearBuffer(WGPUCommandEncoder cSelf, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderClearBufferCmd cmd;

    cmd.self = cSelf;


    
    cmd.buffer = buffer;
    
    cmd.offset = offset;
    
    cmd.size = size;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder cSelf, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderCopyBufferToBufferCmd cmd;

    cmd.self = cSelf;


    
    cmd.source = source;
    
    cmd.sourceOffset = sourceOffset;
    
    cmd.destination = destination;
    
    cmd.destinationOffset = destinationOffset;
    
    cmd.size = size;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderCopyBufferToTexture(WGPUCommandEncoder cSelf, WGPUTexelCopyBufferInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderCopyBufferToTextureCmd cmd;

    cmd.self = cSelf;


    
    cmd.source = source;
    
    cmd.destination = destination;
    
    cmd.copySize = copySize;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder cSelf, WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyBufferInfo const * destination, WGPUExtent3D const * copySize) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderCopyTextureToBufferCmd cmd;

    cmd.self = cSelf;


    
    cmd.source = source;
    
    cmd.destination = destination;
    
    cmd.copySize = copySize;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderCopyTextureToTexture(WGPUCommandEncoder cSelf, WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderCopyTextureToTextureCmd cmd;

    cmd.self = cSelf;


    
    cmd.source = source;
    
    cmd.destination = destination;
    
    cmd.copySize = copySize;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT WGPUCommandBuffer wgpuDawnWireClientCommandEncoderFinish(WGPUCommandEncoder cSelf, WGPUCommandBufferDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderFinishCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::CommandBuffer* returnObject = dawn::wire::client::Create<dawn::wire::client::CommandEncoder*, dawn::wire::client::CommandBuffer>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderInjectValidationError(WGPUCommandEncoder cSelf, WGPUStringView message) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderInjectValidationErrorCmd cmd;

    cmd.self = cSelf;


    
    cmd.message = message;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderInsertDebugMarker(WGPUCommandEncoder cSelf, WGPUStringView markerLabel) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderInsertDebugMarkerCmd cmd;

    cmd.self = cSelf;


    
    cmd.markerLabel = markerLabel;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderPopDebugGroup(WGPUCommandEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderPopDebugGroupCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderPushDebugGroup(WGPUCommandEncoder cSelf, WGPUStringView groupLabel) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderPushDebugGroupCmd cmd;

    cmd.self = cSelf;


    
    cmd.groupLabel = groupLabel;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderResolveQuerySet(WGPUCommandEncoder cSelf, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderResolveQuerySetCmd cmd;

    cmd.self = cSelf;


    
    cmd.querySet = querySet;
    
    cmd.firstQuery = firstQuery;
    
    cmd.queryCount = queryCount;
    
    cmd.destination = destination;
    
    cmd.destinationOffset = destinationOffset;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderSetLabel(WGPUCommandEncoder cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderWriteBuffer(WGPUCommandEncoder cSelf, WGPUBuffer buffer, uint64_t bufferOffset, uint8_t const * data, uint64_t size) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderWriteBufferCmd cmd;

    cmd.self = cSelf;


    
    cmd.buffer = buffer;
    
    cmd.bufferOffset = bufferOffset;
    
    cmd.data = data;
    
    cmd.size = size;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderWriteTimestamp(WGPUCommandEncoder cSelf, WGPUQuerySet querySet, uint32_t queryIndex) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    dawn::wire::CommandEncoderWriteTimestampCmd cmd;

    cmd.self = cSelf;


    
    cmd.querySet = querySet;
    
    cmd.queryIndex = queryIndex;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderAddRef(WGPUCommandEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientCommandEncoderRelease(WGPUCommandEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::CommandEncoder*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder cSelf, uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderDispatchWorkgroupsCmd cmd;

    cmd.self = cSelf;


    
    cmd.workgroupCountX = workgroupCountX;
    
    cmd.workgroupCountY = workgroupCountY;
    
    cmd.workgroupCountZ = workgroupCountZ;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderDispatchWorkgroupsIndirect(WGPUComputePassEncoder cSelf, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderDispatchWorkgroupsIndirectCmd cmd;

    cmd.self = cSelf;


    
    cmd.indirectBuffer = indirectBuffer;
    
    cmd.indirectOffset = indirectOffset;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderEnd(WGPUComputePassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderEndCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder cSelf, WGPUStringView markerLabel) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderInsertDebugMarkerCmd cmd;

    cmd.self = cSelf;


    
    cmd.markerLabel = markerLabel;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderPopDebugGroup(WGPUComputePassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderPopDebugGroupCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderPushDebugGroup(WGPUComputePassEncoder cSelf, WGPUStringView groupLabel) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderPushDebugGroupCmd cmd;

    cmd.self = cSelf;


    
    cmd.groupLabel = groupLabel;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderSetBindGroup(WGPUComputePassEncoder cSelf, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderSetBindGroupCmd cmd;

    cmd.self = cSelf;


    
    cmd.groupIndex = groupIndex;
    
    cmd.group = group;
    
    cmd.dynamicOffsetCount = dynamicOffsetCount;
    
    cmd.dynamicOffsets = dynamicOffsets;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderSetImmediateData(WGPUComputePassEncoder cSelf, uint32_t offset, void const * data, size_t size) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    return self->APISetImmediateData( offset,  data,  size);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderSetLabel(WGPUComputePassEncoder cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderSetPipeline(WGPUComputePassEncoder cSelf, WGPUComputePipeline pipeline) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderSetPipelineCmd cmd;

    cmd.self = cSelf;


    
    cmd.pipeline = pipeline;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderWriteTimestamp(WGPUComputePassEncoder cSelf, WGPUQuerySet querySet, uint32_t queryIndex) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    dawn::wire::ComputePassEncoderWriteTimestampCmd cmd;

    cmd.self = cSelf;


    
    cmd.querySet = querySet;
    
    cmd.queryIndex = queryIndex;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderAddRef(WGPUComputePassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePassEncoderRelease(WGPUComputePassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePassEncoder*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT WGPUBindGroupLayout wgpuDawnWireClientComputePipelineGetBindGroupLayout(WGPUComputePipeline cSelf, uint32_t groupIndex) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePipeline*>(cSelf);
    dawn::wire::ComputePipelineGetBindGroupLayoutCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::BindGroupLayout* returnObject = dawn::wire::client::Create<dawn::wire::client::ComputePipeline*, dawn::wire::client::BindGroupLayout>(self, groupIndex);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.groupIndex = groupIndex;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePipelineSetLabel(WGPUComputePipeline cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePipeline*>(cSelf);
    dawn::wire::ComputePipelineSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePipelineAddRef(WGPUComputePipeline cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePipeline*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientComputePipelineRelease(WGPUComputePipeline cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ComputePipeline*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientDawnDrmFormatCapabilitiesFreeMembers(WGPUDawnDrmFormatCapabilities cSelf) {
    return dawn::wire::client::APIFreeMembers(cSelf);
}


DAWN_WIRE_EXPORT WGPUBindGroup wgpuDawnWireClientDeviceCreateBindGroup(WGPUDevice cSelf, WGPUBindGroupDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateBindGroupCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::BindGroup* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::BindGroup>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUBindGroupLayout wgpuDawnWireClientDeviceCreateBindGroupLayout(WGPUDevice cSelf, WGPUBindGroupLayoutDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateBindGroupLayoutCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::BindGroupLayout* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::BindGroupLayout>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUBuffer wgpuDawnWireClientDeviceCreateBuffer(WGPUDevice cSelf, WGPUBufferDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APICreateBuffer( descriptor);
}

DAWN_WIRE_EXPORT WGPUCommandEncoder wgpuDawnWireClientDeviceCreateCommandEncoder(WGPUDevice cSelf, WGPUCommandEncoderDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateCommandEncoderCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::CommandEncoder* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::CommandEncoder>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUComputePipeline wgpuDawnWireClientDeviceCreateComputePipeline(WGPUDevice cSelf, WGPUComputePipelineDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateComputePipelineCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::ComputePipeline* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::ComputePipeline>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientDeviceCreateComputePipelineAsync(WGPUDevice cSelf, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APICreateComputePipelineAsync( descriptor,  callbackInfo);
}

DAWN_WIRE_EXPORT WGPUBuffer wgpuDawnWireClientDeviceCreateErrorBuffer(WGPUDevice cSelf, WGPUBufferDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APICreateErrorBuffer( descriptor);
}

DAWN_WIRE_EXPORT WGPUExternalTexture wgpuDawnWireClientDeviceCreateErrorExternalTexture(WGPUDevice cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateErrorExternalTextureCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::ExternalTexture* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::ExternalTexture>(self);
    cmd.result = returnObject->GetWireHandle(self->GetClient());


    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUShaderModule wgpuDawnWireClientDeviceCreateErrorShaderModule(WGPUDevice cSelf, WGPUShaderModuleDescriptor const * descriptor, WGPUStringView errorMessage) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateErrorShaderModuleCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::ShaderModule* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::ShaderModule>(self, descriptor, errorMessage);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;
    
    cmd.errorMessage = errorMessage;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUTexture wgpuDawnWireClientDeviceCreateErrorTexture(WGPUDevice cSelf, WGPUTextureDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateErrorTextureCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::Texture* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::Texture>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUExternalTexture wgpuDawnWireClientDeviceCreateExternalTexture(WGPUDevice cSelf, WGPUExternalTextureDescriptor const * externalTextureDescriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateExternalTextureCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::ExternalTexture* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::ExternalTexture>(self, externalTextureDescriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.externalTextureDescriptor = externalTextureDescriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUPipelineLayout wgpuDawnWireClientDeviceCreatePipelineLayout(WGPUDevice cSelf, WGPUPipelineLayoutDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreatePipelineLayoutCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::PipelineLayout* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::PipelineLayout>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUQuerySet wgpuDawnWireClientDeviceCreateQuerySet(WGPUDevice cSelf, WGPUQuerySetDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateQuerySetCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::QuerySet* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::QuerySet>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPURenderBundleEncoder wgpuDawnWireClientDeviceCreateRenderBundleEncoder(WGPUDevice cSelf, WGPURenderBundleEncoderDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateRenderBundleEncoderCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::RenderBundleEncoder* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::RenderBundleEncoder>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPURenderPipeline wgpuDawnWireClientDeviceCreateRenderPipeline(WGPUDevice cSelf, WGPURenderPipelineDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateRenderPipelineCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::RenderPipeline* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::RenderPipeline>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientDeviceCreateRenderPipelineAsync(WGPUDevice cSelf, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APICreateRenderPipelineAsync( descriptor,  callbackInfo);
}

DAWN_WIRE_EXPORT WGPUSampler wgpuDawnWireClientDeviceCreateSampler(WGPUDevice cSelf, WGPUSamplerDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateSamplerCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::Sampler* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::Sampler>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUShaderModule wgpuDawnWireClientDeviceCreateShaderModule(WGPUDevice cSelf, WGPUShaderModuleDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateShaderModuleCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::ShaderModule* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::ShaderModule>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUTexture wgpuDawnWireClientDeviceCreateTexture(WGPUDevice cSelf, WGPUTextureDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceCreateTextureCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::Texture* returnObject = dawn::wire::client::Create<dawn::wire::client::Device*, dawn::wire::client::Texture>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceDestroy(WGPUDevice cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIDestroy();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceForceLoss(WGPUDevice cSelf, WGPUDeviceLostReason type, WGPUStringView message) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceForceLossCmd cmd;

    cmd.self = cSelf;


    
    cmd.type = type;
    
    cmd.message = message;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT WGPUAdapter wgpuDawnWireClientDeviceGetAdapter(WGPUDevice cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIGetAdapter();
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientDeviceGetAdapterInfo(WGPUDevice cSelf, WGPUAdapterInfo * adapterInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIGetAdapterInfo( adapterInfo);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceGetFeatures(WGPUDevice cSelf, WGPUSupportedFeatures * features) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIGetFeatures( features);
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientDeviceGetLimits(WGPUDevice cSelf, WGPULimits * limits) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIGetLimits( limits);
}

DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientDeviceGetLostFuture(WGPUDevice cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIGetLostFuture();
}

DAWN_WIRE_EXPORT WGPUQueue wgpuDawnWireClientDeviceGetQueue(WGPUDevice cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIGetQueue();
}

DAWN_WIRE_EXPORT WGPUBool wgpuDawnWireClientDeviceHasFeature(WGPUDevice cSelf, WGPUFeatureName feature) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIHasFeature( feature);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceInjectError(WGPUDevice cSelf, WGPUErrorType type, WGPUStringView message) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIInjectError( type,  message);
}

DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientDevicePopErrorScope(WGPUDevice cSelf, WGPUPopErrorScopeCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIPopErrorScope( callbackInfo);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDevicePushErrorScope(WGPUDevice cSelf, WGPUErrorFilter filter) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DevicePushErrorScopeCmd cmd;

    cmd.self = cSelf;


    
    cmd.filter = filter;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceSetLabel(WGPUDevice cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceSetLoggingCallback(WGPUDevice cSelf, WGPULoggingCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APISetLoggingCallback( callbackInfo);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceTick(WGPUDevice cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceTickCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceValidateTextureDescriptor(WGPUDevice cSelf, WGPUTextureDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    dawn::wire::DeviceValidateTextureDescriptorCmd cmd;

    cmd.self = cSelf;


    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceAddRef(WGPUDevice cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientDeviceRelease(WGPUDevice cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Device*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientExternalTextureDestroy(WGPUExternalTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ExternalTexture*>(cSelf);
    dawn::wire::ExternalTextureDestroyCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientExternalTextureExpire(WGPUExternalTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ExternalTexture*>(cSelf);
    dawn::wire::ExternalTextureExpireCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientExternalTextureRefresh(WGPUExternalTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ExternalTexture*>(cSelf);
    dawn::wire::ExternalTextureRefreshCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientExternalTextureSetLabel(WGPUExternalTexture cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::ExternalTexture*>(cSelf);
    dawn::wire::ExternalTextureSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientExternalTextureAddRef(WGPUExternalTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ExternalTexture*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientExternalTextureRelease(WGPUExternalTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ExternalTexture*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT WGPUSurface wgpuDawnWireClientInstanceCreateSurface(WGPUInstance cSelf, WGPUSurfaceDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Instance*>(cSelf);
    return self->APICreateSurface( descriptor);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientInstanceGetWGSLLanguageFeatures(WGPUInstance cSelf, WGPUSupportedWGSLLanguageFeatures * features) {
    auto self = reinterpret_cast<dawn::wire::client::Instance*>(cSelf);
    return self->APIGetWGSLLanguageFeatures( features);
}

DAWN_WIRE_EXPORT WGPUBool wgpuDawnWireClientInstanceHasWGSLLanguageFeature(WGPUInstance cSelf, WGPUWGSLLanguageFeatureName feature) {
    auto self = reinterpret_cast<dawn::wire::client::Instance*>(cSelf);
    return self->APIHasWGSLLanguageFeature( feature);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientInstanceProcessEvents(WGPUInstance cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Instance*>(cSelf);
    return self->APIProcessEvents();
}

DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientInstanceRequestAdapter(WGPUInstance cSelf, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Instance*>(cSelf);
    return self->APIRequestAdapter( options,  callbackInfo);
}

DAWN_WIRE_EXPORT WGPUWaitStatus wgpuDawnWireClientInstanceWaitAny(WGPUInstance cSelf, size_t futureCount, WGPUFutureWaitInfo * futures, uint64_t timeoutNS) {
    auto self = reinterpret_cast<dawn::wire::client::Instance*>(cSelf);
    return self->APIWaitAny( futureCount,  futures,  timeoutNS);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientInstanceAddRef(WGPUInstance cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Instance*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientInstanceRelease(WGPUInstance cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Instance*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientPipelineLayoutSetLabel(WGPUPipelineLayout cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::PipelineLayout*>(cSelf);
    dawn::wire::PipelineLayoutSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientPipelineLayoutAddRef(WGPUPipelineLayout cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::PipelineLayout*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientPipelineLayoutRelease(WGPUPipelineLayout cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::PipelineLayout*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientQuerySetDestroy(WGPUQuerySet cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::QuerySet*>(cSelf);
    dawn::wire::QuerySetDestroyCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT uint32_t wgpuDawnWireClientQuerySetGetCount(WGPUQuerySet cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::QuerySet*>(cSelf);
    return self->APIGetCount();
}

DAWN_WIRE_EXPORT WGPUQueryType wgpuDawnWireClientQuerySetGetType(WGPUQuerySet cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::QuerySet*>(cSelf);
    return self->APIGetType();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQuerySetSetLabel(WGPUQuerySet cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::QuerySet*>(cSelf);
    dawn::wire::QuerySetSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQuerySetAddRef(WGPUQuerySet cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::QuerySet*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQuerySetRelease(WGPUQuerySet cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::QuerySet*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientQueueCopyExternalTextureForBrowser(WGPUQueue cSelf, WGPUImageCopyExternalTexture const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize, WGPUCopyTextureForBrowserOptions const * options) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    dawn::wire::QueueCopyExternalTextureForBrowserCmd cmd;

    cmd.self = cSelf;


    
    cmd.source = source;
    
    cmd.destination = destination;
    
    cmd.copySize = copySize;
    
    cmd.options = options;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQueueCopyTextureForBrowser(WGPUQueue cSelf, WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize, WGPUCopyTextureForBrowserOptions const * options) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    dawn::wire::QueueCopyTextureForBrowserCmd cmd;

    cmd.self = cSelf;


    
    cmd.source = source;
    
    cmd.destination = destination;
    
    cmd.copySize = copySize;
    
    cmd.options = options;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientQueueOnSubmittedWorkDone(WGPUQueue cSelf, WGPUQueueWorkDoneCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    return self->APIOnSubmittedWorkDone( callbackInfo);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQueueSetLabel(WGPUQueue cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    dawn::wire::QueueSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQueueSubmit(WGPUQueue cSelf, size_t commandCount, WGPUCommandBuffer const * commands) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    dawn::wire::QueueSubmitCmd cmd;

    cmd.self = cSelf;


    
    cmd.commandCount = commandCount;
    
    cmd.commands = commands;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQueueWriteBuffer(WGPUQueue cSelf, WGPUBuffer buffer, uint64_t bufferOffset, void const * data, size_t size) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    return self->APIWriteBuffer( buffer,  bufferOffset,  data,  size);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQueueWriteTexture(WGPUQueue cSelf, WGPUTexelCopyTextureInfo const * destination, void const * data, size_t dataSize, WGPUTexelCopyBufferLayout const * dataLayout, WGPUExtent3D const * writeSize) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    return self->APIWriteTexture( destination,  data,  dataSize,  dataLayout,  writeSize);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQueueAddRef(WGPUQueue cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientQueueRelease(WGPUQueue cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Queue*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleSetLabel(WGPURenderBundle cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundle*>(cSelf);
    dawn::wire::RenderBundleSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleAddRef(WGPURenderBundle cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundle*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleRelease(WGPURenderBundle cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundle*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderDraw(WGPURenderBundleEncoder cSelf, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderDrawCmd cmd;

    cmd.self = cSelf;


    
    cmd.vertexCount = vertexCount;
    
    cmd.instanceCount = instanceCount;
    
    cmd.firstVertex = firstVertex;
    
    cmd.firstInstance = firstInstance;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder cSelf, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderDrawIndexedCmd cmd;

    cmd.self = cSelf;


    
    cmd.indexCount = indexCount;
    
    cmd.instanceCount = instanceCount;
    
    cmd.firstIndex = firstIndex;
    
    cmd.baseVertex = baseVertex;
    
    cmd.firstInstance = firstInstance;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder cSelf, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderDrawIndexedIndirectCmd cmd;

    cmd.self = cSelf;


    
    cmd.indirectBuffer = indirectBuffer;
    
    cmd.indirectOffset = indirectOffset;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder cSelf, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderDrawIndirectCmd cmd;

    cmd.self = cSelf;


    
    cmd.indirectBuffer = indirectBuffer;
    
    cmd.indirectOffset = indirectOffset;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT WGPURenderBundle wgpuDawnWireClientRenderBundleEncoderFinish(WGPURenderBundleEncoder cSelf, WGPURenderBundleDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderFinishCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::RenderBundle* returnObject = dawn::wire::client::Create<dawn::wire::client::RenderBundleEncoder*, dawn::wire::client::RenderBundle>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder cSelf, WGPUStringView markerLabel) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderInsertDebugMarkerCmd cmd;

    cmd.self = cSelf;


    
    cmd.markerLabel = markerLabel;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderPopDebugGroupCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder cSelf, WGPUStringView groupLabel) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderPushDebugGroupCmd cmd;

    cmd.self = cSelf;


    
    cmd.groupLabel = groupLabel;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder cSelf, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderSetBindGroupCmd cmd;

    cmd.self = cSelf;


    
    cmd.groupIndex = groupIndex;
    
    cmd.group = group;
    
    cmd.dynamicOffsetCount = dynamicOffsetCount;
    
    cmd.dynamicOffsets = dynamicOffsets;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderSetImmediateData(WGPURenderBundleEncoder cSelf, uint32_t offset, void const * data, size_t size) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    return self->APISetImmediateData( offset,  data,  size);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder cSelf, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderSetIndexBufferCmd cmd;

    cmd.self = cSelf;


    
    cmd.buffer = buffer;
    
    cmd.format = format;
    
    cmd.offset = offset;
    
    cmd.size = size;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderSetLabel(WGPURenderBundleEncoder cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder cSelf, WGPURenderPipeline pipeline) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderSetPipelineCmd cmd;

    cmd.self = cSelf;


    
    cmd.pipeline = pipeline;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder cSelf, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    dawn::wire::RenderBundleEncoderSetVertexBufferCmd cmd;

    cmd.self = cSelf;


    
    cmd.slot = slot;
    
    cmd.buffer = buffer;
    
    cmd.offset = offset;
    
    cmd.size = size;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderAddRef(WGPURenderBundleEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderBundleEncoderRelease(WGPURenderBundleEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderBundleEncoder*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder cSelf, uint32_t queryIndex) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderBeginOcclusionQueryCmd cmd;

    cmd.self = cSelf;


    
    cmd.queryIndex = queryIndex;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderDraw(WGPURenderPassEncoder cSelf, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderDrawCmd cmd;

    cmd.self = cSelf;


    
    cmd.vertexCount = vertexCount;
    
    cmd.instanceCount = instanceCount;
    
    cmd.firstVertex = firstVertex;
    
    cmd.firstInstance = firstInstance;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderDrawIndexed(WGPURenderPassEncoder cSelf, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderDrawIndexedCmd cmd;

    cmd.self = cSelf;


    
    cmd.indexCount = indexCount;
    
    cmd.instanceCount = instanceCount;
    
    cmd.firstIndex = firstIndex;
    
    cmd.baseVertex = baseVertex;
    
    cmd.firstInstance = firstInstance;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder cSelf, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderDrawIndexedIndirectCmd cmd;

    cmd.self = cSelf;


    
    cmd.indirectBuffer = indirectBuffer;
    
    cmd.indirectOffset = indirectOffset;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderDrawIndirect(WGPURenderPassEncoder cSelf, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderDrawIndirectCmd cmd;

    cmd.self = cSelf;


    
    cmd.indirectBuffer = indirectBuffer;
    
    cmd.indirectOffset = indirectOffset;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderEnd(WGPURenderPassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderEndCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderEndOcclusionQueryCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderExecuteBundles(WGPURenderPassEncoder cSelf, size_t bundleCount, WGPURenderBundle const * bundles) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderExecuteBundlesCmd cmd;

    cmd.self = cSelf;


    
    cmd.bundleCount = bundleCount;
    
    cmd.bundles = bundles;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder cSelf, WGPUStringView markerLabel) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderInsertDebugMarkerCmd cmd;

    cmd.self = cSelf;


    
    cmd.markerLabel = markerLabel;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderMultiDrawIndexedIndirect(WGPURenderPassEncoder cSelf, WGPUBuffer indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, WGPUBuffer drawCountBuffer, uint64_t drawCountBufferOffset) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderMultiDrawIndexedIndirectCmd cmd;

    cmd.self = cSelf;


    
    cmd.indirectBuffer = indirectBuffer;
    
    cmd.indirectOffset = indirectOffset;
    
    cmd.maxDrawCount = maxDrawCount;
    
    cmd.drawCountBuffer = drawCountBuffer;
    
    cmd.drawCountBufferOffset = drawCountBufferOffset;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderMultiDrawIndirect(WGPURenderPassEncoder cSelf, WGPUBuffer indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, WGPUBuffer drawCountBuffer, uint64_t drawCountBufferOffset) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderMultiDrawIndirectCmd cmd;

    cmd.self = cSelf;


    
    cmd.indirectBuffer = indirectBuffer;
    
    cmd.indirectOffset = indirectOffset;
    
    cmd.maxDrawCount = maxDrawCount;
    
    cmd.drawCountBuffer = drawCountBuffer;
    
    cmd.drawCountBufferOffset = drawCountBufferOffset;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderPixelLocalStorageBarrier(WGPURenderPassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderPixelLocalStorageBarrierCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderPopDebugGroupCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder cSelf, WGPUStringView groupLabel) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderPushDebugGroupCmd cmd;

    cmd.self = cSelf;


    
    cmd.groupLabel = groupLabel;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetBindGroup(WGPURenderPassEncoder cSelf, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetBindGroupCmd cmd;

    cmd.self = cSelf;


    
    cmd.groupIndex = groupIndex;
    
    cmd.group = group;
    
    cmd.dynamicOffsetCount = dynamicOffsetCount;
    
    cmd.dynamicOffsets = dynamicOffsets;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder cSelf, WGPUColor const * color) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetBlendConstantCmd cmd;

    cmd.self = cSelf;


    
    cmd.color = color;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetImmediateData(WGPURenderPassEncoder cSelf, uint32_t offset, void const * data, size_t size) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    return self->APISetImmediateData( offset,  data,  size);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder cSelf, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetIndexBufferCmd cmd;

    cmd.self = cSelf;


    
    cmd.buffer = buffer;
    
    cmd.format = format;
    
    cmd.offset = offset;
    
    cmd.size = size;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetLabel(WGPURenderPassEncoder cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetPipeline(WGPURenderPassEncoder cSelf, WGPURenderPipeline pipeline) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetPipelineCmd cmd;

    cmd.self = cSelf;


    
    cmd.pipeline = pipeline;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetScissorRect(WGPURenderPassEncoder cSelf, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetScissorRectCmd cmd;

    cmd.self = cSelf;


    
    cmd.x = x;
    
    cmd.y = y;
    
    cmd.width = width;
    
    cmd.height = height;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetStencilReference(WGPURenderPassEncoder cSelf, uint32_t reference) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetStencilReferenceCmd cmd;

    cmd.self = cSelf;


    
    cmd.reference = reference;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder cSelf, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetVertexBufferCmd cmd;

    cmd.self = cSelf;


    
    cmd.slot = slot;
    
    cmd.buffer = buffer;
    
    cmd.offset = offset;
    
    cmd.size = size;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderSetViewport(WGPURenderPassEncoder cSelf, float x, float y, float width, float height, float minDepth, float maxDepth) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderSetViewportCmd cmd;

    cmd.self = cSelf;


    
    cmd.x = x;
    
    cmd.y = y;
    
    cmd.width = width;
    
    cmd.height = height;
    
    cmd.minDepth = minDepth;
    
    cmd.maxDepth = maxDepth;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderWriteTimestamp(WGPURenderPassEncoder cSelf, WGPUQuerySet querySet, uint32_t queryIndex) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    dawn::wire::RenderPassEncoderWriteTimestampCmd cmd;

    cmd.self = cSelf;


    
    cmd.querySet = querySet;
    
    cmd.queryIndex = queryIndex;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderAddRef(WGPURenderPassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPassEncoderRelease(WGPURenderPassEncoder cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPassEncoder*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT WGPUBindGroupLayout wgpuDawnWireClientRenderPipelineGetBindGroupLayout(WGPURenderPipeline cSelf, uint32_t groupIndex) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPipeline*>(cSelf);
    dawn::wire::RenderPipelineGetBindGroupLayoutCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::BindGroupLayout* returnObject = dawn::wire::client::Create<dawn::wire::client::RenderPipeline*, dawn::wire::client::BindGroupLayout>(self, groupIndex);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.groupIndex = groupIndex;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPipelineSetLabel(WGPURenderPipeline cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPipeline*>(cSelf);
    dawn::wire::RenderPipelineSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPipelineAddRef(WGPURenderPipeline cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPipeline*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientRenderPipelineRelease(WGPURenderPipeline cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::RenderPipeline*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientSamplerSetLabel(WGPUSampler cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::Sampler*>(cSelf);
    dawn::wire::SamplerSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientSamplerAddRef(WGPUSampler cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Sampler*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientSamplerRelease(WGPUSampler cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Sampler*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT WGPUFuture wgpuDawnWireClientShaderModuleGetCompilationInfo(WGPUShaderModule cSelf, WGPUCompilationInfoCallbackInfo callbackInfo) {
    auto self = reinterpret_cast<dawn::wire::client::ShaderModule*>(cSelf);
    return self->APIGetCompilationInfo( callbackInfo);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientShaderModuleSetLabel(WGPUShaderModule cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::ShaderModule*>(cSelf);
    dawn::wire::ShaderModuleSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientShaderModuleAddRef(WGPUShaderModule cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ShaderModule*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientShaderModuleRelease(WGPUShaderModule cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::ShaderModule*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientSupportedFeaturesFreeMembers(WGPUSupportedFeatures cSelf) {
    return dawn::wire::client::APIFreeMembers(cSelf);
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientSupportedInstanceFeaturesFreeMembers(WGPUSupportedInstanceFeatures cSelf) {
    return dawn::wire::client::APIFreeMembers(cSelf);
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientSupportedWGSLLanguageFeaturesFreeMembers(WGPUSupportedWGSLLanguageFeatures cSelf) {
    return dawn::wire::client::APIFreeMembers(cSelf);
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientSurfaceConfigure(WGPUSurface cSelf, WGPUSurfaceConfiguration const * config) {
    auto self = reinterpret_cast<dawn::wire::client::Surface*>(cSelf);
    return self->APIConfigure( config);
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientSurfaceGetCapabilities(WGPUSurface cSelf, WGPUAdapter adapter, WGPUSurfaceCapabilities * capabilities) {
    auto self = reinterpret_cast<dawn::wire::client::Surface*>(cSelf);
    return self->APIGetCapabilities( adapter,  capabilities);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientSurfaceGetCurrentTexture(WGPUSurface cSelf, WGPUSurfaceTexture * surfaceTexture) {
    auto self = reinterpret_cast<dawn::wire::client::Surface*>(cSelf);
    return self->APIGetCurrentTexture( surfaceTexture);
}

DAWN_WIRE_EXPORT WGPUStatus wgpuDawnWireClientSurfacePresent(WGPUSurface cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Surface*>(cSelf);
    return self->APIPresent();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientSurfaceSetLabel(WGPUSurface cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::Surface*>(cSelf);
    dawn::wire::SurfaceSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientSurfaceUnconfigure(WGPUSurface cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Surface*>(cSelf);
    return self->APIUnconfigure();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientSurfaceAddRef(WGPUSurface cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Surface*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientSurfaceRelease(WGPUSurface cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Surface*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities cSelf) {
    return dawn::wire::client::APIFreeMembers(cSelf);
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientTexelBufferViewSetLabel(WGPUTexelBufferView cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::TexelBufferView*>(cSelf);
    dawn::wire::TexelBufferViewSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTexelBufferViewAddRef(WGPUTexelBufferView cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::TexelBufferView*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTexelBufferViewRelease(WGPUTexelBufferView cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::TexelBufferView*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT WGPUTextureView wgpuDawnWireClientTextureCreateErrorView(WGPUTexture cSelf, WGPUTextureViewDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    dawn::wire::TextureCreateErrorViewCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::TextureView* returnObject = dawn::wire::client::Create<dawn::wire::client::Texture*, dawn::wire::client::TextureView>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT WGPUTextureView wgpuDawnWireClientTextureCreateView(WGPUTexture cSelf, WGPUTextureViewDescriptor const * descriptor) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    dawn::wire::TextureCreateViewCmd cmd;

    cmd.self = cSelf;

    dawn::wire::client::TextureView* returnObject = dawn::wire::client::Create<dawn::wire::client::Texture*, dawn::wire::client::TextureView>(self, descriptor);
    cmd.result = returnObject->GetWireHandle(self->GetClient());

    
    cmd.descriptor = descriptor;

    self->GetClient()->SerializeCommand(cmd);

    return ToAPI(returnObject);
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTextureDestroy(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    dawn::wire::TextureDestroyCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT uint32_t wgpuDawnWireClientTextureGetDepthOrArrayLayers(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIGetDepthOrArrayLayers();
}

DAWN_WIRE_EXPORT WGPUTextureDimension wgpuDawnWireClientTextureGetDimension(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIGetDimension();
}

DAWN_WIRE_EXPORT WGPUTextureFormat wgpuDawnWireClientTextureGetFormat(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIGetFormat();
}

DAWN_WIRE_EXPORT uint32_t wgpuDawnWireClientTextureGetHeight(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIGetHeight();
}

DAWN_WIRE_EXPORT uint32_t wgpuDawnWireClientTextureGetMipLevelCount(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIGetMipLevelCount();
}

DAWN_WIRE_EXPORT uint32_t wgpuDawnWireClientTextureGetSampleCount(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIGetSampleCount();
}

DAWN_WIRE_EXPORT WGPUTextureUsage wgpuDawnWireClientTextureGetUsage(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIGetUsage();
}

DAWN_WIRE_EXPORT uint32_t wgpuDawnWireClientTextureGetWidth(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIGetWidth();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTexturePin(WGPUTexture cSelf, WGPUTextureUsage usage) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    dawn::wire::TexturePinCmd cmd;

    cmd.self = cSelf;


    
    cmd.usage = usage;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTextureSetLabel(WGPUTexture cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    dawn::wire::TextureSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTextureUnpin(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    dawn::wire::TextureUnpinCmd cmd;

    cmd.self = cSelf;



    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTextureAddRef(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTextureRelease(WGPUTexture cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::Texture*>(cSelf);
    return self->APIRelease();
}


DAWN_WIRE_EXPORT void wgpuDawnWireClientTextureViewSetLabel(WGPUTextureView cSelf, WGPUStringView label) {
    auto self = reinterpret_cast<dawn::wire::client::TextureView*>(cSelf);
    dawn::wire::TextureViewSetLabelCmd cmd;

    cmd.self = cSelf;


    
    cmd.label = label;

    self->GetClient()->SerializeCommand(cmd);

}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTextureViewAddRef(WGPUTextureView cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::TextureView*>(cSelf);
    return self->APIAddRef();
}

DAWN_WIRE_EXPORT void wgpuDawnWireClientTextureViewRelease(WGPUTextureView cSelf) {
    auto self = reinterpret_cast<dawn::wire::client::TextureView*>(cSelf);
    return self->APIRelease();
}


namespace {
    struct ProcEntry {
        WGPUProc proc;
        std::string_view name;
    };
    static const ProcEntry sProcMap[] = {
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterAddRef), "wgpuAdapterAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterCreateDevice), "wgpuAdapterCreateDevice" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterGetFeatures), "wgpuAdapterGetFeatures" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterGetFormatCapabilities), "wgpuAdapterGetFormatCapabilities" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterGetInfo), "wgpuAdapterGetInfo" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterGetInstance), "wgpuAdapterGetInstance" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterGetLimits), "wgpuAdapterGetLimits" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterHasFeature), "wgpuAdapterHasFeature" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterInfoFreeMembers), "wgpuAdapterInfoFreeMembers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterPropertiesMemoryHeapsFreeMembers), "wgpuAdapterPropertiesMemoryHeapsFreeMembers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterPropertiesSubgroupMatrixConfigsFreeMembers), "wgpuAdapterPropertiesSubgroupMatrixConfigsFreeMembers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterRelease), "wgpuAdapterRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientAdapterRequestDevice), "wgpuAdapterRequestDevice" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBindGroupAddRef), "wgpuBindGroupAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBindGroupDestroy), "wgpuBindGroupDestroy" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBindGroupLayoutAddRef), "wgpuBindGroupLayoutAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBindGroupLayoutRelease), "wgpuBindGroupLayoutRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBindGroupLayoutSetLabel), "wgpuBindGroupLayoutSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBindGroupRelease), "wgpuBindGroupRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBindGroupSetLabel), "wgpuBindGroupSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferAddRef), "wgpuBufferAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferCreateTexelView), "wgpuBufferCreateTexelView" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferDestroy), "wgpuBufferDestroy" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferGetConstMappedRange), "wgpuBufferGetConstMappedRange" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferGetMapState), "wgpuBufferGetMapState" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferGetMappedRange), "wgpuBufferGetMappedRange" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferGetSize), "wgpuBufferGetSize" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferGetUsage), "wgpuBufferGetUsage" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferMapAsync), "wgpuBufferMapAsync" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferReadMappedRange), "wgpuBufferReadMappedRange" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferRelease), "wgpuBufferRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferSetLabel), "wgpuBufferSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferUnmap), "wgpuBufferUnmap" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientBufferWriteMappedRange), "wgpuBufferWriteMappedRange" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandBufferAddRef), "wgpuCommandBufferAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandBufferRelease), "wgpuCommandBufferRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandBufferSetLabel), "wgpuCommandBufferSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderAddRef), "wgpuCommandEncoderAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderBeginComputePass), "wgpuCommandEncoderBeginComputePass" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderBeginRenderPass), "wgpuCommandEncoderBeginRenderPass" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderClearBuffer), "wgpuCommandEncoderClearBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderCopyBufferToBuffer), "wgpuCommandEncoderCopyBufferToBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderCopyBufferToTexture), "wgpuCommandEncoderCopyBufferToTexture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderCopyTextureToBuffer), "wgpuCommandEncoderCopyTextureToBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderCopyTextureToTexture), "wgpuCommandEncoderCopyTextureToTexture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderFinish), "wgpuCommandEncoderFinish" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderInjectValidationError), "wgpuCommandEncoderInjectValidationError" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderInsertDebugMarker), "wgpuCommandEncoderInsertDebugMarker" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderPopDebugGroup), "wgpuCommandEncoderPopDebugGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderPushDebugGroup), "wgpuCommandEncoderPushDebugGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderRelease), "wgpuCommandEncoderRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderResolveQuerySet), "wgpuCommandEncoderResolveQuerySet" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderSetLabel), "wgpuCommandEncoderSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderWriteBuffer), "wgpuCommandEncoderWriteBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientCommandEncoderWriteTimestamp), "wgpuCommandEncoderWriteTimestamp" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderAddRef), "wgpuComputePassEncoderAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderDispatchWorkgroups), "wgpuComputePassEncoderDispatchWorkgroups" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderDispatchWorkgroupsIndirect), "wgpuComputePassEncoderDispatchWorkgroupsIndirect" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderEnd), "wgpuComputePassEncoderEnd" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderInsertDebugMarker), "wgpuComputePassEncoderInsertDebugMarker" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderPopDebugGroup), "wgpuComputePassEncoderPopDebugGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderPushDebugGroup), "wgpuComputePassEncoderPushDebugGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderRelease), "wgpuComputePassEncoderRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderSetBindGroup), "wgpuComputePassEncoderSetBindGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderSetImmediateData), "wgpuComputePassEncoderSetImmediateData" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderSetLabel), "wgpuComputePassEncoderSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderSetPipeline), "wgpuComputePassEncoderSetPipeline" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePassEncoderWriteTimestamp), "wgpuComputePassEncoderWriteTimestamp" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePipelineAddRef), "wgpuComputePipelineAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePipelineGetBindGroupLayout), "wgpuComputePipelineGetBindGroupLayout" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePipelineRelease), "wgpuComputePipelineRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientComputePipelineSetLabel), "wgpuComputePipelineSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDawnDrmFormatCapabilitiesFreeMembers), "wgpuDawnDrmFormatCapabilitiesFreeMembers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceAddRef), "wgpuDeviceAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateBindGroup), "wgpuDeviceCreateBindGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateBindGroupLayout), "wgpuDeviceCreateBindGroupLayout" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateBuffer), "wgpuDeviceCreateBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateCommandEncoder), "wgpuDeviceCreateCommandEncoder" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateComputePipeline), "wgpuDeviceCreateComputePipeline" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateComputePipelineAsync), "wgpuDeviceCreateComputePipelineAsync" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateErrorBuffer), "wgpuDeviceCreateErrorBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateErrorExternalTexture), "wgpuDeviceCreateErrorExternalTexture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateErrorShaderModule), "wgpuDeviceCreateErrorShaderModule" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateErrorTexture), "wgpuDeviceCreateErrorTexture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateExternalTexture), "wgpuDeviceCreateExternalTexture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreatePipelineLayout), "wgpuDeviceCreatePipelineLayout" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateQuerySet), "wgpuDeviceCreateQuerySet" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateRenderBundleEncoder), "wgpuDeviceCreateRenderBundleEncoder" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateRenderPipeline), "wgpuDeviceCreateRenderPipeline" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateRenderPipelineAsync), "wgpuDeviceCreateRenderPipelineAsync" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateSampler), "wgpuDeviceCreateSampler" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateShaderModule), "wgpuDeviceCreateShaderModule" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceCreateTexture), "wgpuDeviceCreateTexture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceDestroy), "wgpuDeviceDestroy" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceForceLoss), "wgpuDeviceForceLoss" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceGetAdapter), "wgpuDeviceGetAdapter" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceGetAdapterInfo), "wgpuDeviceGetAdapterInfo" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceGetFeatures), "wgpuDeviceGetFeatures" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceGetLimits), "wgpuDeviceGetLimits" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceGetLostFuture), "wgpuDeviceGetLostFuture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceGetQueue), "wgpuDeviceGetQueue" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceHasFeature), "wgpuDeviceHasFeature" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceInjectError), "wgpuDeviceInjectError" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDevicePopErrorScope), "wgpuDevicePopErrorScope" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDevicePushErrorScope), "wgpuDevicePushErrorScope" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceRelease), "wgpuDeviceRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceSetLabel), "wgpuDeviceSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceSetLoggingCallback), "wgpuDeviceSetLoggingCallback" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceTick), "wgpuDeviceTick" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientDeviceValidateTextureDescriptor), "wgpuDeviceValidateTextureDescriptor" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientExternalTextureAddRef), "wgpuExternalTextureAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientExternalTextureDestroy), "wgpuExternalTextureDestroy" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientExternalTextureExpire), "wgpuExternalTextureExpire" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientExternalTextureRefresh), "wgpuExternalTextureRefresh" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientExternalTextureRelease), "wgpuExternalTextureRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientExternalTextureSetLabel), "wgpuExternalTextureSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientInstanceAddRef), "wgpuInstanceAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientInstanceCreateSurface), "wgpuInstanceCreateSurface" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientInstanceGetWGSLLanguageFeatures), "wgpuInstanceGetWGSLLanguageFeatures" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientInstanceHasWGSLLanguageFeature), "wgpuInstanceHasWGSLLanguageFeature" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientInstanceProcessEvents), "wgpuInstanceProcessEvents" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientInstanceRelease), "wgpuInstanceRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientInstanceRequestAdapter), "wgpuInstanceRequestAdapter" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientInstanceWaitAny), "wgpuInstanceWaitAny" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientPipelineLayoutAddRef), "wgpuPipelineLayoutAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientPipelineLayoutRelease), "wgpuPipelineLayoutRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientPipelineLayoutSetLabel), "wgpuPipelineLayoutSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQuerySetAddRef), "wgpuQuerySetAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQuerySetDestroy), "wgpuQuerySetDestroy" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQuerySetGetCount), "wgpuQuerySetGetCount" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQuerySetGetType), "wgpuQuerySetGetType" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQuerySetRelease), "wgpuQuerySetRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQuerySetSetLabel), "wgpuQuerySetSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueAddRef), "wgpuQueueAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueCopyExternalTextureForBrowser), "wgpuQueueCopyExternalTextureForBrowser" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueCopyTextureForBrowser), "wgpuQueueCopyTextureForBrowser" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueOnSubmittedWorkDone), "wgpuQueueOnSubmittedWorkDone" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueRelease), "wgpuQueueRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueSetLabel), "wgpuQueueSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueSubmit), "wgpuQueueSubmit" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueWriteBuffer), "wgpuQueueWriteBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientQueueWriteTexture), "wgpuQueueWriteTexture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleAddRef), "wgpuRenderBundleAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderAddRef), "wgpuRenderBundleEncoderAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderDraw), "wgpuRenderBundleEncoderDraw" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderDrawIndexed), "wgpuRenderBundleEncoderDrawIndexed" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderDrawIndexedIndirect), "wgpuRenderBundleEncoderDrawIndexedIndirect" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderDrawIndirect), "wgpuRenderBundleEncoderDrawIndirect" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderFinish), "wgpuRenderBundleEncoderFinish" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderInsertDebugMarker), "wgpuRenderBundleEncoderInsertDebugMarker" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderPopDebugGroup), "wgpuRenderBundleEncoderPopDebugGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderPushDebugGroup), "wgpuRenderBundleEncoderPushDebugGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderRelease), "wgpuRenderBundleEncoderRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderSetBindGroup), "wgpuRenderBundleEncoderSetBindGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderSetImmediateData), "wgpuRenderBundleEncoderSetImmediateData" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderSetIndexBuffer), "wgpuRenderBundleEncoderSetIndexBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderSetLabel), "wgpuRenderBundleEncoderSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderSetPipeline), "wgpuRenderBundleEncoderSetPipeline" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleEncoderSetVertexBuffer), "wgpuRenderBundleEncoderSetVertexBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleRelease), "wgpuRenderBundleRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderBundleSetLabel), "wgpuRenderBundleSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderAddRef), "wgpuRenderPassEncoderAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderBeginOcclusionQuery), "wgpuRenderPassEncoderBeginOcclusionQuery" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderDraw), "wgpuRenderPassEncoderDraw" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderDrawIndexed), "wgpuRenderPassEncoderDrawIndexed" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderDrawIndexedIndirect), "wgpuRenderPassEncoderDrawIndexedIndirect" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderDrawIndirect), "wgpuRenderPassEncoderDrawIndirect" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderEnd), "wgpuRenderPassEncoderEnd" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderEndOcclusionQuery), "wgpuRenderPassEncoderEndOcclusionQuery" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderExecuteBundles), "wgpuRenderPassEncoderExecuteBundles" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderInsertDebugMarker), "wgpuRenderPassEncoderInsertDebugMarker" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderMultiDrawIndexedIndirect), "wgpuRenderPassEncoderMultiDrawIndexedIndirect" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderMultiDrawIndirect), "wgpuRenderPassEncoderMultiDrawIndirect" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderPixelLocalStorageBarrier), "wgpuRenderPassEncoderPixelLocalStorageBarrier" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderPopDebugGroup), "wgpuRenderPassEncoderPopDebugGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderPushDebugGroup), "wgpuRenderPassEncoderPushDebugGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderRelease), "wgpuRenderPassEncoderRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetBindGroup), "wgpuRenderPassEncoderSetBindGroup" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetBlendConstant), "wgpuRenderPassEncoderSetBlendConstant" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetImmediateData), "wgpuRenderPassEncoderSetImmediateData" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetIndexBuffer), "wgpuRenderPassEncoderSetIndexBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetLabel), "wgpuRenderPassEncoderSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetPipeline), "wgpuRenderPassEncoderSetPipeline" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetScissorRect), "wgpuRenderPassEncoderSetScissorRect" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetStencilReference), "wgpuRenderPassEncoderSetStencilReference" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetVertexBuffer), "wgpuRenderPassEncoderSetVertexBuffer" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderSetViewport), "wgpuRenderPassEncoderSetViewport" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPassEncoderWriteTimestamp), "wgpuRenderPassEncoderWriteTimestamp" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPipelineAddRef), "wgpuRenderPipelineAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPipelineGetBindGroupLayout), "wgpuRenderPipelineGetBindGroupLayout" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPipelineRelease), "wgpuRenderPipelineRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientRenderPipelineSetLabel), "wgpuRenderPipelineSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSamplerAddRef), "wgpuSamplerAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSamplerRelease), "wgpuSamplerRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSamplerSetLabel), "wgpuSamplerSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientShaderModuleAddRef), "wgpuShaderModuleAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientShaderModuleGetCompilationInfo), "wgpuShaderModuleGetCompilationInfo" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientShaderModuleRelease), "wgpuShaderModuleRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientShaderModuleSetLabel), "wgpuShaderModuleSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSupportedFeaturesFreeMembers), "wgpuSupportedFeaturesFreeMembers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSupportedInstanceFeaturesFreeMembers), "wgpuSupportedInstanceFeaturesFreeMembers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSupportedWGSLLanguageFeaturesFreeMembers), "wgpuSupportedWGSLLanguageFeaturesFreeMembers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfaceAddRef), "wgpuSurfaceAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfaceCapabilitiesFreeMembers), "wgpuSurfaceCapabilitiesFreeMembers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfaceConfigure), "wgpuSurfaceConfigure" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfaceGetCapabilities), "wgpuSurfaceGetCapabilities" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfaceGetCurrentTexture), "wgpuSurfaceGetCurrentTexture" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfacePresent), "wgpuSurfacePresent" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfaceRelease), "wgpuSurfaceRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfaceSetLabel), "wgpuSurfaceSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientSurfaceUnconfigure), "wgpuSurfaceUnconfigure" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTexelBufferViewAddRef), "wgpuTexelBufferViewAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTexelBufferViewRelease), "wgpuTexelBufferViewRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTexelBufferViewSetLabel), "wgpuTexelBufferViewSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureAddRef), "wgpuTextureAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureCreateErrorView), "wgpuTextureCreateErrorView" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureCreateView), "wgpuTextureCreateView" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureDestroy), "wgpuTextureDestroy" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureGetDepthOrArrayLayers), "wgpuTextureGetDepthOrArrayLayers" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureGetDimension), "wgpuTextureGetDimension" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureGetFormat), "wgpuTextureGetFormat" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureGetHeight), "wgpuTextureGetHeight" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureGetMipLevelCount), "wgpuTextureGetMipLevelCount" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureGetSampleCount), "wgpuTextureGetSampleCount" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureGetUsage), "wgpuTextureGetUsage" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureGetWidth), "wgpuTextureGetWidth" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTexturePin), "wgpuTexturePin" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureRelease), "wgpuTextureRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureSetLabel), "wgpuTextureSetLabel" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureUnpin), "wgpuTextureUnpin" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureViewAddRef), "wgpuTextureViewAddRef" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureViewRelease), "wgpuTextureViewRelease" },
        { reinterpret_cast<WGPUProc>(wgpuDawnWireClientTextureViewSetLabel), "wgpuTextureViewSetLabel" },
    };
    static constexpr size_t sProcMapSize = sizeof(sProcMap) / sizeof(sProcMap[0]);
}  // anonymous namespace

DAWN_WIRE_EXPORT WGPUProc wgpuDawnWireClientGetProcAddress(WGPUStringView cProcName) {
    if (cProcName.data == nullptr) {
        return nullptr;
    }

    std::string_view procName(cProcName.data, cProcName.length != WGPU_STRLEN ? cProcName.length : strlen(cProcName.data));

    const ProcEntry* entry = std::lower_bound(&sProcMap[0], &sProcMap[sProcMapSize], procName,
        [](const ProcEntry &a, const std::string_view& b) -> bool {
            return a.name.compare(b) < 0;
        }
    );

    if (entry != &sProcMap[sProcMapSize] && entry->name == procName) {
        return entry->proc;
    }

    // Special case the free-standing functions of the API.
    // TODO(dawn:1238) Checking string one by one is slow, it needs to be optimized.
    if (procName == "wgpuCreateInstance") {
        return reinterpret_cast<WGPUProc>(wgpuDawnWireClientCreateInstance);
    }

    if (procName == "wgpuGetInstanceFeatures") {
        return reinterpret_cast<WGPUProc>(wgpuDawnWireClientGetInstanceFeatures);
    }

    if (procName == "wgpuGetInstanceLimits") {
        return reinterpret_cast<WGPUProc>(wgpuDawnWireClientGetInstanceLimits);
    }

    if (procName == "wgpuHasInstanceFeature") {
        return reinterpret_cast<WGPUProc>(wgpuDawnWireClientHasInstanceFeature);
    }

    if (procName == "wgpuGetProcAddress") {
        return reinterpret_cast<WGPUProc>(wgpuDawnWireClientGetProcAddress);
    }

    return nullptr;
}

namespace dawn::wire::client {

    std::vector<std::string_view> GetProcMapNamesForTesting() {
        std::vector<std::string_view> result;
        result.reserve(sProcMapSize);
        for (const ProcEntry& entry : sProcMap) {
            result.push_back(entry.name);
        }
        return result;
    }


    constexpr DawnProcTable MakeProcTable() {
        DawnProcTable procs = {};
        procs.createInstance = wgpuDawnWireClientCreateInstance;
        procs.getInstanceFeatures = wgpuDawnWireClientGetInstanceFeatures;
        procs.getInstanceLimits = wgpuDawnWireClientGetInstanceLimits;
        procs.hasInstanceFeature = wgpuDawnWireClientHasInstanceFeature;
        procs.getProcAddress = wgpuDawnWireClientGetProcAddress;
        procs.adapterCreateDevice = wgpuDawnWireClientAdapterCreateDevice;
        procs.adapterGetFeatures = wgpuDawnWireClientAdapterGetFeatures;
        procs.adapterGetFormatCapabilities = wgpuDawnWireClientAdapterGetFormatCapabilities;
        procs.adapterGetInfo = wgpuDawnWireClientAdapterGetInfo;
        procs.adapterGetInstance = wgpuDawnWireClientAdapterGetInstance;
        procs.adapterGetLimits = wgpuDawnWireClientAdapterGetLimits;
        procs.adapterHasFeature = wgpuDawnWireClientAdapterHasFeature;
        procs.adapterRequestDevice = wgpuDawnWireClientAdapterRequestDevice;
        procs.adapterAddRef = wgpuDawnWireClientAdapterAddRef;
        procs.adapterRelease = wgpuDawnWireClientAdapterRelease;
        procs.adapterInfoFreeMembers = wgpuDawnWireClientAdapterInfoFreeMembers;
        procs.adapterPropertiesMemoryHeapsFreeMembers = wgpuDawnWireClientAdapterPropertiesMemoryHeapsFreeMembers;
        procs.adapterPropertiesSubgroupMatrixConfigsFreeMembers = wgpuDawnWireClientAdapterPropertiesSubgroupMatrixConfigsFreeMembers;
        procs.bindGroupDestroy = wgpuDawnWireClientBindGroupDestroy;
        procs.bindGroupSetLabel = wgpuDawnWireClientBindGroupSetLabel;
        procs.bindGroupAddRef = wgpuDawnWireClientBindGroupAddRef;
        procs.bindGroupRelease = wgpuDawnWireClientBindGroupRelease;
        procs.bindGroupLayoutSetLabel = wgpuDawnWireClientBindGroupLayoutSetLabel;
        procs.bindGroupLayoutAddRef = wgpuDawnWireClientBindGroupLayoutAddRef;
        procs.bindGroupLayoutRelease = wgpuDawnWireClientBindGroupLayoutRelease;
        procs.bufferCreateTexelView = wgpuDawnWireClientBufferCreateTexelView;
        procs.bufferDestroy = wgpuDawnWireClientBufferDestroy;
        procs.bufferGetConstMappedRange = wgpuDawnWireClientBufferGetConstMappedRange;
        procs.bufferGetMappedRange = wgpuDawnWireClientBufferGetMappedRange;
        procs.bufferGetMapState = wgpuDawnWireClientBufferGetMapState;
        procs.bufferGetSize = wgpuDawnWireClientBufferGetSize;
        procs.bufferGetUsage = wgpuDawnWireClientBufferGetUsage;
        procs.bufferMapAsync = wgpuDawnWireClientBufferMapAsync;
        procs.bufferReadMappedRange = wgpuDawnWireClientBufferReadMappedRange;
        procs.bufferSetLabel = wgpuDawnWireClientBufferSetLabel;
        procs.bufferUnmap = wgpuDawnWireClientBufferUnmap;
        procs.bufferWriteMappedRange = wgpuDawnWireClientBufferWriteMappedRange;
        procs.bufferAddRef = wgpuDawnWireClientBufferAddRef;
        procs.bufferRelease = wgpuDawnWireClientBufferRelease;
        procs.commandBufferSetLabel = wgpuDawnWireClientCommandBufferSetLabel;
        procs.commandBufferAddRef = wgpuDawnWireClientCommandBufferAddRef;
        procs.commandBufferRelease = wgpuDawnWireClientCommandBufferRelease;
        procs.commandEncoderBeginComputePass = wgpuDawnWireClientCommandEncoderBeginComputePass;
        procs.commandEncoderBeginRenderPass = wgpuDawnWireClientCommandEncoderBeginRenderPass;
        procs.commandEncoderClearBuffer = wgpuDawnWireClientCommandEncoderClearBuffer;
        procs.commandEncoderCopyBufferToBuffer = wgpuDawnWireClientCommandEncoderCopyBufferToBuffer;
        procs.commandEncoderCopyBufferToTexture = wgpuDawnWireClientCommandEncoderCopyBufferToTexture;
        procs.commandEncoderCopyTextureToBuffer = wgpuDawnWireClientCommandEncoderCopyTextureToBuffer;
        procs.commandEncoderCopyTextureToTexture = wgpuDawnWireClientCommandEncoderCopyTextureToTexture;
        procs.commandEncoderFinish = wgpuDawnWireClientCommandEncoderFinish;
        procs.commandEncoderInjectValidationError = wgpuDawnWireClientCommandEncoderInjectValidationError;
        procs.commandEncoderInsertDebugMarker = wgpuDawnWireClientCommandEncoderInsertDebugMarker;
        procs.commandEncoderPopDebugGroup = wgpuDawnWireClientCommandEncoderPopDebugGroup;
        procs.commandEncoderPushDebugGroup = wgpuDawnWireClientCommandEncoderPushDebugGroup;
        procs.commandEncoderResolveQuerySet = wgpuDawnWireClientCommandEncoderResolveQuerySet;
        procs.commandEncoderSetLabel = wgpuDawnWireClientCommandEncoderSetLabel;
        procs.commandEncoderWriteBuffer = wgpuDawnWireClientCommandEncoderWriteBuffer;
        procs.commandEncoderWriteTimestamp = wgpuDawnWireClientCommandEncoderWriteTimestamp;
        procs.commandEncoderAddRef = wgpuDawnWireClientCommandEncoderAddRef;
        procs.commandEncoderRelease = wgpuDawnWireClientCommandEncoderRelease;
        procs.computePassEncoderDispatchWorkgroups = wgpuDawnWireClientComputePassEncoderDispatchWorkgroups;
        procs.computePassEncoderDispatchWorkgroupsIndirect = wgpuDawnWireClientComputePassEncoderDispatchWorkgroupsIndirect;
        procs.computePassEncoderEnd = wgpuDawnWireClientComputePassEncoderEnd;
        procs.computePassEncoderInsertDebugMarker = wgpuDawnWireClientComputePassEncoderInsertDebugMarker;
        procs.computePassEncoderPopDebugGroup = wgpuDawnWireClientComputePassEncoderPopDebugGroup;
        procs.computePassEncoderPushDebugGroup = wgpuDawnWireClientComputePassEncoderPushDebugGroup;
        procs.computePassEncoderSetBindGroup = wgpuDawnWireClientComputePassEncoderSetBindGroup;
        procs.computePassEncoderSetImmediateData = wgpuDawnWireClientComputePassEncoderSetImmediateData;
        procs.computePassEncoderSetLabel = wgpuDawnWireClientComputePassEncoderSetLabel;
        procs.computePassEncoderSetPipeline = wgpuDawnWireClientComputePassEncoderSetPipeline;
        procs.computePassEncoderWriteTimestamp = wgpuDawnWireClientComputePassEncoderWriteTimestamp;
        procs.computePassEncoderAddRef = wgpuDawnWireClientComputePassEncoderAddRef;
        procs.computePassEncoderRelease = wgpuDawnWireClientComputePassEncoderRelease;
        procs.computePipelineGetBindGroupLayout = wgpuDawnWireClientComputePipelineGetBindGroupLayout;
        procs.computePipelineSetLabel = wgpuDawnWireClientComputePipelineSetLabel;
        procs.computePipelineAddRef = wgpuDawnWireClientComputePipelineAddRef;
        procs.computePipelineRelease = wgpuDawnWireClientComputePipelineRelease;
        procs.dawnDrmFormatCapabilitiesFreeMembers = wgpuDawnWireClientDawnDrmFormatCapabilitiesFreeMembers;
        procs.deviceCreateBindGroup = wgpuDawnWireClientDeviceCreateBindGroup;
        procs.deviceCreateBindGroupLayout = wgpuDawnWireClientDeviceCreateBindGroupLayout;
        procs.deviceCreateBuffer = wgpuDawnWireClientDeviceCreateBuffer;
        procs.deviceCreateCommandEncoder = wgpuDawnWireClientDeviceCreateCommandEncoder;
        procs.deviceCreateComputePipeline = wgpuDawnWireClientDeviceCreateComputePipeline;
        procs.deviceCreateComputePipelineAsync = wgpuDawnWireClientDeviceCreateComputePipelineAsync;
        procs.deviceCreateErrorBuffer = wgpuDawnWireClientDeviceCreateErrorBuffer;
        procs.deviceCreateErrorExternalTexture = wgpuDawnWireClientDeviceCreateErrorExternalTexture;
        procs.deviceCreateErrorShaderModule = wgpuDawnWireClientDeviceCreateErrorShaderModule;
        procs.deviceCreateErrorTexture = wgpuDawnWireClientDeviceCreateErrorTexture;
        procs.deviceCreateExternalTexture = wgpuDawnWireClientDeviceCreateExternalTexture;
        procs.deviceCreatePipelineLayout = wgpuDawnWireClientDeviceCreatePipelineLayout;
        procs.deviceCreateQuerySet = wgpuDawnWireClientDeviceCreateQuerySet;
        procs.deviceCreateRenderBundleEncoder = wgpuDawnWireClientDeviceCreateRenderBundleEncoder;
        procs.deviceCreateRenderPipeline = wgpuDawnWireClientDeviceCreateRenderPipeline;
        procs.deviceCreateRenderPipelineAsync = wgpuDawnWireClientDeviceCreateRenderPipelineAsync;
        procs.deviceCreateSampler = wgpuDawnWireClientDeviceCreateSampler;
        procs.deviceCreateShaderModule = wgpuDawnWireClientDeviceCreateShaderModule;
        procs.deviceCreateTexture = wgpuDawnWireClientDeviceCreateTexture;
        procs.deviceDestroy = wgpuDawnWireClientDeviceDestroy;
        procs.deviceForceLoss = wgpuDawnWireClientDeviceForceLoss;
        procs.deviceGetAdapter = wgpuDawnWireClientDeviceGetAdapter;
        procs.deviceGetAdapterInfo = wgpuDawnWireClientDeviceGetAdapterInfo;
        procs.deviceGetFeatures = wgpuDawnWireClientDeviceGetFeatures;
        procs.deviceGetLimits = wgpuDawnWireClientDeviceGetLimits;
        procs.deviceGetLostFuture = wgpuDawnWireClientDeviceGetLostFuture;
        procs.deviceGetQueue = wgpuDawnWireClientDeviceGetQueue;
        procs.deviceHasFeature = wgpuDawnWireClientDeviceHasFeature;
        procs.deviceInjectError = wgpuDawnWireClientDeviceInjectError;
        procs.devicePopErrorScope = wgpuDawnWireClientDevicePopErrorScope;
        procs.devicePushErrorScope = wgpuDawnWireClientDevicePushErrorScope;
        procs.deviceSetLabel = wgpuDawnWireClientDeviceSetLabel;
        procs.deviceSetLoggingCallback = wgpuDawnWireClientDeviceSetLoggingCallback;
        procs.deviceTick = wgpuDawnWireClientDeviceTick;
        procs.deviceValidateTextureDescriptor = wgpuDawnWireClientDeviceValidateTextureDescriptor;
        procs.deviceAddRef = wgpuDawnWireClientDeviceAddRef;
        procs.deviceRelease = wgpuDawnWireClientDeviceRelease;
        procs.externalTextureDestroy = wgpuDawnWireClientExternalTextureDestroy;
        procs.externalTextureExpire = wgpuDawnWireClientExternalTextureExpire;
        procs.externalTextureRefresh = wgpuDawnWireClientExternalTextureRefresh;
        procs.externalTextureSetLabel = wgpuDawnWireClientExternalTextureSetLabel;
        procs.externalTextureAddRef = wgpuDawnWireClientExternalTextureAddRef;
        procs.externalTextureRelease = wgpuDawnWireClientExternalTextureRelease;
        procs.instanceCreateSurface = wgpuDawnWireClientInstanceCreateSurface;
        procs.instanceGetWGSLLanguageFeatures = wgpuDawnWireClientInstanceGetWGSLLanguageFeatures;
        procs.instanceHasWGSLLanguageFeature = wgpuDawnWireClientInstanceHasWGSLLanguageFeature;
        procs.instanceProcessEvents = wgpuDawnWireClientInstanceProcessEvents;
        procs.instanceRequestAdapter = wgpuDawnWireClientInstanceRequestAdapter;
        procs.instanceWaitAny = wgpuDawnWireClientInstanceWaitAny;
        procs.instanceAddRef = wgpuDawnWireClientInstanceAddRef;
        procs.instanceRelease = wgpuDawnWireClientInstanceRelease;
        procs.pipelineLayoutSetLabel = wgpuDawnWireClientPipelineLayoutSetLabel;
        procs.pipelineLayoutAddRef = wgpuDawnWireClientPipelineLayoutAddRef;
        procs.pipelineLayoutRelease = wgpuDawnWireClientPipelineLayoutRelease;
        procs.querySetDestroy = wgpuDawnWireClientQuerySetDestroy;
        procs.querySetGetCount = wgpuDawnWireClientQuerySetGetCount;
        procs.querySetGetType = wgpuDawnWireClientQuerySetGetType;
        procs.querySetSetLabel = wgpuDawnWireClientQuerySetSetLabel;
        procs.querySetAddRef = wgpuDawnWireClientQuerySetAddRef;
        procs.querySetRelease = wgpuDawnWireClientQuerySetRelease;
        procs.queueCopyExternalTextureForBrowser = wgpuDawnWireClientQueueCopyExternalTextureForBrowser;
        procs.queueCopyTextureForBrowser = wgpuDawnWireClientQueueCopyTextureForBrowser;
        procs.queueOnSubmittedWorkDone = wgpuDawnWireClientQueueOnSubmittedWorkDone;
        procs.queueSetLabel = wgpuDawnWireClientQueueSetLabel;
        procs.queueSubmit = wgpuDawnWireClientQueueSubmit;
        procs.queueWriteBuffer = wgpuDawnWireClientQueueWriteBuffer;
        procs.queueWriteTexture = wgpuDawnWireClientQueueWriteTexture;
        procs.queueAddRef = wgpuDawnWireClientQueueAddRef;
        procs.queueRelease = wgpuDawnWireClientQueueRelease;
        procs.renderBundleSetLabel = wgpuDawnWireClientRenderBundleSetLabel;
        procs.renderBundleAddRef = wgpuDawnWireClientRenderBundleAddRef;
        procs.renderBundleRelease = wgpuDawnWireClientRenderBundleRelease;
        procs.renderBundleEncoderDraw = wgpuDawnWireClientRenderBundleEncoderDraw;
        procs.renderBundleEncoderDrawIndexed = wgpuDawnWireClientRenderBundleEncoderDrawIndexed;
        procs.renderBundleEncoderDrawIndexedIndirect = wgpuDawnWireClientRenderBundleEncoderDrawIndexedIndirect;
        procs.renderBundleEncoderDrawIndirect = wgpuDawnWireClientRenderBundleEncoderDrawIndirect;
        procs.renderBundleEncoderFinish = wgpuDawnWireClientRenderBundleEncoderFinish;
        procs.renderBundleEncoderInsertDebugMarker = wgpuDawnWireClientRenderBundleEncoderInsertDebugMarker;
        procs.renderBundleEncoderPopDebugGroup = wgpuDawnWireClientRenderBundleEncoderPopDebugGroup;
        procs.renderBundleEncoderPushDebugGroup = wgpuDawnWireClientRenderBundleEncoderPushDebugGroup;
        procs.renderBundleEncoderSetBindGroup = wgpuDawnWireClientRenderBundleEncoderSetBindGroup;
        procs.renderBundleEncoderSetImmediateData = wgpuDawnWireClientRenderBundleEncoderSetImmediateData;
        procs.renderBundleEncoderSetIndexBuffer = wgpuDawnWireClientRenderBundleEncoderSetIndexBuffer;
        procs.renderBundleEncoderSetLabel = wgpuDawnWireClientRenderBundleEncoderSetLabel;
        procs.renderBundleEncoderSetPipeline = wgpuDawnWireClientRenderBundleEncoderSetPipeline;
        procs.renderBundleEncoderSetVertexBuffer = wgpuDawnWireClientRenderBundleEncoderSetVertexBuffer;
        procs.renderBundleEncoderAddRef = wgpuDawnWireClientRenderBundleEncoderAddRef;
        procs.renderBundleEncoderRelease = wgpuDawnWireClientRenderBundleEncoderRelease;
        procs.renderPassEncoderBeginOcclusionQuery = wgpuDawnWireClientRenderPassEncoderBeginOcclusionQuery;
        procs.renderPassEncoderDraw = wgpuDawnWireClientRenderPassEncoderDraw;
        procs.renderPassEncoderDrawIndexed = wgpuDawnWireClientRenderPassEncoderDrawIndexed;
        procs.renderPassEncoderDrawIndexedIndirect = wgpuDawnWireClientRenderPassEncoderDrawIndexedIndirect;
        procs.renderPassEncoderDrawIndirect = wgpuDawnWireClientRenderPassEncoderDrawIndirect;
        procs.renderPassEncoderEnd = wgpuDawnWireClientRenderPassEncoderEnd;
        procs.renderPassEncoderEndOcclusionQuery = wgpuDawnWireClientRenderPassEncoderEndOcclusionQuery;
        procs.renderPassEncoderExecuteBundles = wgpuDawnWireClientRenderPassEncoderExecuteBundles;
        procs.renderPassEncoderInsertDebugMarker = wgpuDawnWireClientRenderPassEncoderInsertDebugMarker;
        procs.renderPassEncoderMultiDrawIndexedIndirect = wgpuDawnWireClientRenderPassEncoderMultiDrawIndexedIndirect;
        procs.renderPassEncoderMultiDrawIndirect = wgpuDawnWireClientRenderPassEncoderMultiDrawIndirect;
        procs.renderPassEncoderPixelLocalStorageBarrier = wgpuDawnWireClientRenderPassEncoderPixelLocalStorageBarrier;
        procs.renderPassEncoderPopDebugGroup = wgpuDawnWireClientRenderPassEncoderPopDebugGroup;
        procs.renderPassEncoderPushDebugGroup = wgpuDawnWireClientRenderPassEncoderPushDebugGroup;
        procs.renderPassEncoderSetBindGroup = wgpuDawnWireClientRenderPassEncoderSetBindGroup;
        procs.renderPassEncoderSetBlendConstant = wgpuDawnWireClientRenderPassEncoderSetBlendConstant;
        procs.renderPassEncoderSetImmediateData = wgpuDawnWireClientRenderPassEncoderSetImmediateData;
        procs.renderPassEncoderSetIndexBuffer = wgpuDawnWireClientRenderPassEncoderSetIndexBuffer;
        procs.renderPassEncoderSetLabel = wgpuDawnWireClientRenderPassEncoderSetLabel;
        procs.renderPassEncoderSetPipeline = wgpuDawnWireClientRenderPassEncoderSetPipeline;
        procs.renderPassEncoderSetScissorRect = wgpuDawnWireClientRenderPassEncoderSetScissorRect;
        procs.renderPassEncoderSetStencilReference = wgpuDawnWireClientRenderPassEncoderSetStencilReference;
        procs.renderPassEncoderSetVertexBuffer = wgpuDawnWireClientRenderPassEncoderSetVertexBuffer;
        procs.renderPassEncoderSetViewport = wgpuDawnWireClientRenderPassEncoderSetViewport;
        procs.renderPassEncoderWriteTimestamp = wgpuDawnWireClientRenderPassEncoderWriteTimestamp;
        procs.renderPassEncoderAddRef = wgpuDawnWireClientRenderPassEncoderAddRef;
        procs.renderPassEncoderRelease = wgpuDawnWireClientRenderPassEncoderRelease;
        procs.renderPipelineGetBindGroupLayout = wgpuDawnWireClientRenderPipelineGetBindGroupLayout;
        procs.renderPipelineSetLabel = wgpuDawnWireClientRenderPipelineSetLabel;
        procs.renderPipelineAddRef = wgpuDawnWireClientRenderPipelineAddRef;
        procs.renderPipelineRelease = wgpuDawnWireClientRenderPipelineRelease;
        procs.samplerSetLabel = wgpuDawnWireClientSamplerSetLabel;
        procs.samplerAddRef = wgpuDawnWireClientSamplerAddRef;
        procs.samplerRelease = wgpuDawnWireClientSamplerRelease;
        procs.shaderModuleGetCompilationInfo = wgpuDawnWireClientShaderModuleGetCompilationInfo;
        procs.shaderModuleSetLabel = wgpuDawnWireClientShaderModuleSetLabel;
        procs.shaderModuleAddRef = wgpuDawnWireClientShaderModuleAddRef;
        procs.shaderModuleRelease = wgpuDawnWireClientShaderModuleRelease;
        procs.supportedFeaturesFreeMembers = wgpuDawnWireClientSupportedFeaturesFreeMembers;
        procs.supportedInstanceFeaturesFreeMembers = wgpuDawnWireClientSupportedInstanceFeaturesFreeMembers;
        procs.supportedWGSLLanguageFeaturesFreeMembers = wgpuDawnWireClientSupportedWGSLLanguageFeaturesFreeMembers;
        procs.surfaceConfigure = wgpuDawnWireClientSurfaceConfigure;
        procs.surfaceGetCapabilities = wgpuDawnWireClientSurfaceGetCapabilities;
        procs.surfaceGetCurrentTexture = wgpuDawnWireClientSurfaceGetCurrentTexture;
        procs.surfacePresent = wgpuDawnWireClientSurfacePresent;
        procs.surfaceSetLabel = wgpuDawnWireClientSurfaceSetLabel;
        procs.surfaceUnconfigure = wgpuDawnWireClientSurfaceUnconfigure;
        procs.surfaceAddRef = wgpuDawnWireClientSurfaceAddRef;
        procs.surfaceRelease = wgpuDawnWireClientSurfaceRelease;
        procs.surfaceCapabilitiesFreeMembers = wgpuDawnWireClientSurfaceCapabilitiesFreeMembers;
        procs.texelBufferViewSetLabel = wgpuDawnWireClientTexelBufferViewSetLabel;
        procs.texelBufferViewAddRef = wgpuDawnWireClientTexelBufferViewAddRef;
        procs.texelBufferViewRelease = wgpuDawnWireClientTexelBufferViewRelease;
        procs.textureCreateErrorView = wgpuDawnWireClientTextureCreateErrorView;
        procs.textureCreateView = wgpuDawnWireClientTextureCreateView;
        procs.textureDestroy = wgpuDawnWireClientTextureDestroy;
        procs.textureGetDepthOrArrayLayers = wgpuDawnWireClientTextureGetDepthOrArrayLayers;
        procs.textureGetDimension = wgpuDawnWireClientTextureGetDimension;
        procs.textureGetFormat = wgpuDawnWireClientTextureGetFormat;
        procs.textureGetHeight = wgpuDawnWireClientTextureGetHeight;
        procs.textureGetMipLevelCount = wgpuDawnWireClientTextureGetMipLevelCount;
        procs.textureGetSampleCount = wgpuDawnWireClientTextureGetSampleCount;
        procs.textureGetUsage = wgpuDawnWireClientTextureGetUsage;
        procs.textureGetWidth = wgpuDawnWireClientTextureGetWidth;
        procs.texturePin = wgpuDawnWireClientTexturePin;
        procs.textureSetLabel = wgpuDawnWireClientTextureSetLabel;
        procs.textureUnpin = wgpuDawnWireClientTextureUnpin;
        procs.textureAddRef = wgpuDawnWireClientTextureAddRef;
        procs.textureRelease = wgpuDawnWireClientTextureRelease;
        procs.textureViewSetLabel = wgpuDawnWireClientTextureViewSetLabel;
        procs.textureViewAddRef = wgpuDawnWireClientTextureViewAddRef;
        procs.textureViewRelease = wgpuDawnWireClientTextureViewRelease;
        return procs;
    }

    static DawnProcTable gProcTable = MakeProcTable();

    const DawnProcTable& GetProcs() {
        return gProcTable;
    }

}  // namespace dawn::wire::client
