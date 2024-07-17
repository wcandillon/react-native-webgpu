#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUQuerySetDescriptor {
  wgpu::QueryType type;             /* GPUQueryType */
  double count;                     /* GPUSize32 */
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu