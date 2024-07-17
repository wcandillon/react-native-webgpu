#pragma once

#include <optional>

namespace rnwgpu {

struct GPUBufferBinding {
  unknown buffer;               // GPUBuffer
  std::optional<double> offset; // GPUSize64
  std::optional<double> size;   // GPUSize64
};

} // namespace rnwgpu