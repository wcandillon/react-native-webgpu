#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUBindGroupLayoutDescriptor {
  unknown entries;                  /* Iterable<GPUBindGroupLayoutEntry> */
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu