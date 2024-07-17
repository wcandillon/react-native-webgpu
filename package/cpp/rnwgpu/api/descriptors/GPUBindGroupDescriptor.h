#pragma once

#include <vector>
#include <string>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupLayout.h"
#include "GPUBindGroupEntry.h"

namespace rnwgpu {

struct GPUBindGroupDescriptor {
  std::shared_ptr<GPUBindGroupLayout> layout; /* GPUBindGroupLayout */
  std::vector<std::shared_ptr<GPUBindGroupEntry>> entries; /* Iterable<GPUBindGroupEntry> */
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu