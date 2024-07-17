#pragma once

#include <optional>

namespace rnwgpu {

struct GPUStencilFaceState {
  std::optional<unknown> compare;     // GPUCompareFunction
  std::optional<unknown> failOp;      // GPUStencilOperation
  std::optional<unknown> depthFailOp; // GPUStencilOperation
  std::optional<unknown> passOp;      // GPUStencilOperation
};

} // namespace rnwgpu