#pragma once

#include <optional>

namespace rnwgpu {

struct GPUColorTargetState {
  unknown format;                  // GPUTextureFormat
  std::optional<unknown> blend;    // GPUBlendState
  std::optional<double> writeMask; // GPUColorWriteFlags
};

} // namespace rnwgpu