#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUBlendComponent {
  std::optional<wgpu::BlendOperation> operation; // GPUBlendOperation
  std::optional<wgpu::BlendFactor> srcFactor;    // GPUBlendFactor
  std::optional<wgpu::BlendFactor> dstFactor;    // GPUBlendFactor
};

} // namespace rnwgpu