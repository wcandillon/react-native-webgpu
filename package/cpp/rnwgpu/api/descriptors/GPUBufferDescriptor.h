#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUBufferDescriptor {
  double size;                          // GPUSize64
  double usage;                         // GPUBufferUsageFlags
  std::optional<bool> mappedAtCreation; // boolean
  std::optional<std::string> label;     // string
};

} // namespace rnwgpu