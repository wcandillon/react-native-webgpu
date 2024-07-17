#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUQuerySetDescriptor {
  unknown type;                     // GPUQueryType
  double count;                     // GPUSize32
  std::optional<std::string> label; // string
};

} // namespace rnwgpu