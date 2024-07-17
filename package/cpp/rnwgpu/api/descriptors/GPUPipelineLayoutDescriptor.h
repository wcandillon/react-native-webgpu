#pragma once

#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupLayout.h"

namespace rnwgpu {

struct GPUPipelineLayoutDescriptor {
  std::vector<std::shared_ptr<GPUBindGroupLayout>>
      bindGroupLayouts;             /* Iterable<GPUBindGroupLayout> */
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu