#pragma once

#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupEntry.h"
#include "GPUBindGroupLayout.h"

namespace rnwgpu {

struct GPUBindGroupDescriptor {
  std::shared_ptr<GPUBindGroupLayout> layout; // GPUBindGroupLayout
  std::vector<std::shared_ptr<GPUBindGroupEntry>>
      entries;                      // Iterable<GPUBindGroupEntry>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu