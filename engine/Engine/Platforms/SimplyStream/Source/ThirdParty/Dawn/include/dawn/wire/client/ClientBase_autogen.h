
#ifndef DAWNWIRE_CLIENT_CLIENTBASE_AUTOGEN_H_
#define DAWNWIRE_CLIENT_CLIENTBASE_AUTOGEN_H_

#include "dawn/wire/ChunkedCommandHandler.h"
#include "dawn/wire/WireCmd_autogen.h"
#include "dawn/wire/client/ApiObjects.h"

namespace dawn::wire::client {

    class ClientBase : public ChunkedCommandHandler, public ObjectIdProvider {
      public:
        ClientBase() = default;
        ~ClientBase() override = default;

      private:
        // Implementation of the ObjectIdProvider interface
        WireResult GetId(WGPUAdapter object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<Adapter*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUAdapter object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<Adapter*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUBindGroup object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<BindGroup*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUBindGroup object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<BindGroup*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUBindGroupLayout object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<BindGroupLayout*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUBindGroupLayout object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<BindGroupLayout*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUBuffer object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<Buffer*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUBuffer object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<Buffer*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUCommandBuffer object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<CommandBuffer*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUCommandBuffer object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<CommandBuffer*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUCommandEncoder object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<CommandEncoder*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUCommandEncoder object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<CommandEncoder*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUComputePassEncoder object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<ComputePassEncoder*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUComputePassEncoder object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<ComputePassEncoder*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUComputePipeline object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<ComputePipeline*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUComputePipeline object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<ComputePipeline*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUDevice object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<Device*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUDevice object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<Device*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUExternalTexture object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<ExternalTexture*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUExternalTexture object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<ExternalTexture*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUInstance object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<Instance*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUInstance object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<Instance*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUPipelineLayout object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<PipelineLayout*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUPipelineLayout object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<PipelineLayout*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUQuerySet object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<QuerySet*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUQuerySet object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<QuerySet*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUQueue object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<Queue*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUQueue object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<Queue*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPURenderBundle object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<RenderBundle*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPURenderBundle object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<RenderBundle*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPURenderBundleEncoder object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<RenderBundleEncoder*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPURenderBundleEncoder object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<RenderBundleEncoder*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPURenderPassEncoder object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<RenderPassEncoder*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPURenderPassEncoder object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<RenderPassEncoder*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPURenderPipeline object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<RenderPipeline*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPURenderPipeline object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<RenderPipeline*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUSampler object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<Sampler*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUSampler object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<Sampler*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUShaderModule object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<ShaderModule*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUShaderModule object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<ShaderModule*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUSurface object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<Surface*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUSurface object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<Surface*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUTexelBufferView object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<TexelBufferView*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUTexelBufferView object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<TexelBufferView*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUTexture object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<Texture*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUTexture object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<Texture*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
        WireResult GetId(WGPUTextureView object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            if (object == nullptr) {
                return WireResult::FatalError;
            }
            *out = reinterpret_cast<TextureView*>(object)->GetWireHandle(this).id;
            return WireResult::Success;
        }
        WireResult GetOptionalId(WGPUTextureView object, ObjectId* out) const final {
            DAWN_ASSERT(out != nullptr);
            *out = (object == nullptr ? 0 : reinterpret_cast<TextureView*>(object)->GetWireHandle(this).id);
            return WireResult::Success;
        }
    };

}  // namespace dawn::wire::client

#endif  // DAWNWIRE_CLIENT_CLIENTBASE_AUTOGEN_H_
