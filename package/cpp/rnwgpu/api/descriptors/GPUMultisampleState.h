#pragma once

#include <optional>

namespace rnwgpu {

struct GPUMultisampleState {
  std::optional<double> count;                // GPUSize32
  std::optional<double> mask;                 // GPUSampleMask
  std::optional<bool> alphaToCoverageEnabled; // boolean
};

} // namespace rnwgpu