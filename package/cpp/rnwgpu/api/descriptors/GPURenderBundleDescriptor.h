#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPURenderBundleDescriptor {
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu