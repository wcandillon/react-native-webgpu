#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUBindGroupDescriptor {
  unknown layout;                   // GPUBindGroupLayout
  unknown entries;                  // Iterable<GPUBindGroupEntry>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu