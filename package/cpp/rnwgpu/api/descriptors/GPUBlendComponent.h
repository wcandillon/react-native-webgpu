#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUBlendComponent {
  std::optional<wgpu::> operation; /* GPUBlendOperation */
  std::optional<wgpu::> srcFactor; /* GPUBlendFactor */
  std::optional<wgpu::> dstFactor; /* GPUBlendFactor */
};

} // namespace rnwgpu