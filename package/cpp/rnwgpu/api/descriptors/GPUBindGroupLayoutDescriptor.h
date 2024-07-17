#pragma once

#include <vector>
#include <string>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupLayoutEntry.h"

namespace rnwgpu {

struct GPUBindGroupLayoutDescriptor {
  std::vector<std::shared_ptr<GPUBindGroupLayoutEntry>> entries; /* Iterable<GPUBindGroupLayoutEntry> */
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu