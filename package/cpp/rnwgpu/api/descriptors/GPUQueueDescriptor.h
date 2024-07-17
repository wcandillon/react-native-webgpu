#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUQueueDescriptor {
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu