#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUSamplerBindingLayout {
  std::optional<wgpu::SamplerBindingType> type; /* GPUSamplerBindingType */
};

} // namespace rnwgpu