#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUComputePassTimestampWrites.h"

namespace rnwgpu {

struct GPUComputePassDescriptor {
  std::optional<std::shared_ptr<GPUComputePassTimestampWrites>>
      timestampWrites;              // GPUComputePassTimestampWrites
  std::optional<std::string> label; // string
};

} // namespace rnwgpu