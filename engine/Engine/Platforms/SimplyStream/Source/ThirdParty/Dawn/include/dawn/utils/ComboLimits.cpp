#include "dawn/utils/ComboLimits.h"

namespace dawn::utils {

ComboLimits::ComboLimits() = default;

void ComboLimits::UnlinkedCopyTo(ComboLimits* dst) const {
    *static_cast<wgpu::Limits*>(dst) = *this;
    dst->wgpu::Limits::nextInChain = nullptr;
    *static_cast<wgpu::CompatibilityModeLimits*>(dst) = *this;
    dst->wgpu::CompatibilityModeLimits::nextInChain = nullptr;
}

}  // namespace dawn::utils
