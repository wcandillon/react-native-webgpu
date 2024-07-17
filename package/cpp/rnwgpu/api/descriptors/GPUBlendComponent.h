#pragma once

#include <optional>

namespace rnwgpu {

struct GPUBlendComponent {
  std::optional<unknown> operation; // GPUBlendOperation
  std::optional<unknown> srcFactor; // GPUBlendFactor
  std::optional<unknown> dstFactor; // GPUBlendFactor
};

} // namespace rnwgpu