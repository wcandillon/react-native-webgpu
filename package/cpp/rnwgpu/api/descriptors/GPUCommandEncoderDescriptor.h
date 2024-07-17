#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUCommandEncoderDescriptor {
  std::optional<std::string> label; // string
};

} // namespace rnwgpu