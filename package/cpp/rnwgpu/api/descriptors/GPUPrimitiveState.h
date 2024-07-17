#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUPrimitiveState {
  std::optional<wgpu::> topology;         /* GPUPrimitiveTopology */
  std::optional<wgpu::> stripIndexFormat; /* GPUIndexFormat */
  std::optional<wgpu::> frontFace;        /* GPUFrontFace */
  std::optional<wgpu::> cullMode;         /* GPUCullMode */
  std::optional<bool> unclippedDepth;     /* boolean */
};

} // namespace rnwgpu