#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUPipelineLayoutDescriptor {
  unknown bindGroupLayouts;         // Iterable<GPUBindGroupLayout>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu