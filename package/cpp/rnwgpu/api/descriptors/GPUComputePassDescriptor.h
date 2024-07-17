#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUComputePassDescriptor {
  std::optional<unknown> timestampWrites; /* GPUComputePassTimestampWrites */
  std::optional<std::string> label;       /* string */
};

} // namespace rnwgpu