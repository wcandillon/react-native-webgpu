#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUQuerySet.h"

namespace rnwgpu {

struct GPUComputePassTimestampWrites {
  std::shared_ptr<GPUQuerySet> querySet;           // GPUQuerySet
  std::optional<double> beginningOfPassWriteIndex; // GPUSize32
  std::optional<double> endOfPassWriteIndex;       // GPUSize32
};

} // namespace rnwgpu