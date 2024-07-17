#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUQueueDescriptor {
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu