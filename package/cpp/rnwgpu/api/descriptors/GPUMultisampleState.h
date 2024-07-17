#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"



namespace rnwgpu {

struct GPUMultisampleState {
  std::optional<double> count; /* GPUSize32 */
  std::optional<double> mask; /* GPUSampleMask */
  std::optional<bool> alphaToCoverageEnabled; /* boolean */
};

} // namespace rnwgpu