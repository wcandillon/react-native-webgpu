#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUBufferDescriptor {
  double size;                          /* GPUSize64 */
  double usage;                         /* GPUBufferUsageFlags */
  std::optional<bool> mappedAtCreation; /* boolean */
  std::optional<std::string> label;     /* string */
};

} // namespace rnwgpu