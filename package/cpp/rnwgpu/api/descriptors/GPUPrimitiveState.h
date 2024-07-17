#pragma once

#include <optional>

namespace rnwgpu {

struct GPUPrimitiveState {
  std::optional<unknown> topology;         // GPUPrimitiveTopology
  std::optional<unknown> stripIndexFormat; // GPUIndexFormat
  std::optional<unknown> frontFace;        // GPUFrontFace
  std::optional<unknown> cullMode;         // GPUCullMode
  std::optional<bool> unclippedDepth;      // boolean
};

} // namespace rnwgpu