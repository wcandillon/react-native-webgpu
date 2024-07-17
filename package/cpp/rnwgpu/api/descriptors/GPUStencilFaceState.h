#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUStencilFaceState {
  std::optional<wgpu::> compare;     /* GPUCompareFunction */
  std::optional<wgpu::> failOp;      /* GPUStencilOperation */
  std::optional<wgpu::> depthFailOp; /* GPUStencilOperation */
  std::optional<wgpu::> passOp;      /* GPUStencilOperation */
};

} // namespace rnwgpu