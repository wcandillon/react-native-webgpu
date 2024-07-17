#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUCommandEncoderDescriptor {
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu