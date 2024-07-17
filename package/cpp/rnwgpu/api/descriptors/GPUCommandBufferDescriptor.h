#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUCommandBufferDescriptor {
  std::optional<std::string> label; // string
};

} // namespace rnwgpu