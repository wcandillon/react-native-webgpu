#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUPrimitiveState {
  std::optional<wgpu::PrimitiveTopology> topology;   // GPUPrimitiveTopology
  std::optional<wgpu::IndexFormat> stripIndexFormat; // GPUIndexFormat
  std::optional<wgpu::FrontFace> frontFace;          // GPUFrontFace
  std::optional<wgpu::CullMode> cullMode;            // GPUCullMode
  std::optional<bool> unclippedDepth;                // boolean
};

} // namespace rnwgpu