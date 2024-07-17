#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUSamplerBindingLayout {
  std::optional<wgpu::SamplerBindingType> type; // GPUSamplerBindingType
};

} // namespace rnwgpu