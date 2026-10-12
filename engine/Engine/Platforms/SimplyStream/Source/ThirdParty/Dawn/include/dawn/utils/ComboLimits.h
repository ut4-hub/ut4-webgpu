#ifndef SRC_DAWN_UTILS_COMBOLIMITS_H_
#define SRC_DAWN_UTILS_COMBOLIMITS_H_

#include <webgpu/webgpu_cpp.h>

#include <concepts>

#include "dawn/common/NonMovable.h"

namespace dawn::utils {

class ComboLimits : public NonMovable
        , private wgpu::Limits
        , private wgpu::CompatibilityModeLimits
    {
  public:
    ComboLimits();

    // This is not copyable or movable to avoid surprises with nextInChain pointers becoming stale
    // (or getting replaced with nullptr). This explicit copy makes it clear what happens.
    void UnlinkedCopyTo(ComboLimits*) const;

    // Modify the ComboLimits in-place to link the extension structs correctly, and return the base
    // struct. Optionally accepts any number of additional structs to add to the
    // end of the chain, e.g.: `comboLimits.GetLinked(&extension1, &extension2)`.
    // Always use GetLinked (rather than `&comboLimits`) whenever passing a ComboLimits to the API.
    template <typename... Extension>
        requires (std::convertible_to<Extension, wgpu::ChainedStructOut*> && ...)
    wgpu::Limits* GetLinked(Extension... extension) {
        wgpu::ChainedStructOut* lastExtension = nullptr;
        // Link all of the standard extensions.
        lastExtension = this->wgpu::Limits::nextInChain =
            static_cast<wgpu::CompatibilityModeLimits*>(this);
        // Link any extensions passed by the caller.
        ((lastExtension = lastExtension->nextInChain = extension), ...);
        lastExtension->nextInChain = nullptr;
        return this;
    }

    using wgpu::Limits::maxTextureDimension1D;
    using wgpu::Limits::maxTextureDimension2D;
    using wgpu::Limits::maxTextureDimension3D;
    using wgpu::Limits::maxTextureArrayLayers;
    using wgpu::Limits::maxBindGroups;
    using wgpu::Limits::maxBindGroupsPlusVertexBuffers;
    using wgpu::Limits::maxBindingsPerBindGroup;
    using wgpu::Limits::maxDynamicUniformBuffersPerPipelineLayout;
    using wgpu::Limits::maxDynamicStorageBuffersPerPipelineLayout;
    using wgpu::Limits::maxSampledTexturesPerShaderStage;
    using wgpu::Limits::maxSamplersPerShaderStage;
    using wgpu::Limits::maxStorageBuffersPerShaderStage;
    using wgpu::Limits::maxStorageTexturesPerShaderStage;
    using wgpu::Limits::maxUniformBuffersPerShaderStage;
    using wgpu::Limits::maxUniformBufferBindingSize;
    using wgpu::Limits::maxStorageBufferBindingSize;
    using wgpu::Limits::minUniformBufferOffsetAlignment;
    using wgpu::Limits::minStorageBufferOffsetAlignment;
    using wgpu::Limits::maxVertexBuffers;
    using wgpu::Limits::maxBufferSize;
    using wgpu::Limits::maxVertexAttributes;
    using wgpu::Limits::maxVertexBufferArrayStride;
    using wgpu::Limits::maxInterStageShaderVariables;
    using wgpu::Limits::maxColorAttachments;
    using wgpu::Limits::maxColorAttachmentBytesPerSample;
    using wgpu::Limits::maxComputeWorkgroupStorageSize;
    using wgpu::Limits::maxComputeInvocationsPerWorkgroup;
    using wgpu::Limits::maxComputeWorkgroupSizeX;
    using wgpu::Limits::maxComputeWorkgroupSizeY;
    using wgpu::Limits::maxComputeWorkgroupSizeZ;
    using wgpu::Limits::maxComputeWorkgroupsPerDimension;
    using wgpu::Limits::maxImmediateSize;
    using wgpu::CompatibilityModeLimits::maxStorageBuffersInVertexStage;
    using wgpu::CompatibilityModeLimits::maxStorageTexturesInVertexStage;
    using wgpu::CompatibilityModeLimits::maxStorageBuffersInFragmentStage;
    using wgpu::CompatibilityModeLimits::maxStorageTexturesInFragmentStage;
};

}  // namespace dawn::utils

#endif  // SRC_DAWN_UTILS_COMBOLIMITS_H_
