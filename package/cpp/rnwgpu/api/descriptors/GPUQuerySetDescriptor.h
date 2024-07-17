#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUQuerySetDescriptor {
  wgpu::QueryType type;             // GPUQueryType
  double count;                     // GPUSize32
  std::optional<std::string> label; // string
};

} // namespace rnwgpu