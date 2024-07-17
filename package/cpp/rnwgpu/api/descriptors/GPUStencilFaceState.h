#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUStencilFaceState {
  std::optional<wgpu::CompareFunction> compare;      /* GPUCompareFunction */
  std::optional<wgpu::StencilOperation> failOp;      /* GPUStencilOperation */
  std::optional<wgpu::StencilOperation> depthFailOp; /* GPUStencilOperation */
  std::optional<wgpu::StencilOperation> passOp;      /* GPUStencilOperation */
};

} // namespace rnwgpu