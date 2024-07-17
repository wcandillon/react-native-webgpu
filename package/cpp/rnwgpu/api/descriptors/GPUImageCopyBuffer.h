#pragma once

#include <optional>

namespace rnwgpu {

struct GPUImageCopyBuffer {
  unknown buffer;                     // GPUBuffer
  std::optional<double> offset;       // GPUSize64
  std::optional<double> bytesPerRow;  // GPUSize32
  std::optional<double> rowsPerImage; // GPUSize32
};

} // namespace rnwgpu