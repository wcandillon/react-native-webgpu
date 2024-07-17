#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPURenderPassTimestampWrites {
  unknown querySet;                                /* GPUQuerySet */
  std::optional<double> beginningOfPassWriteIndex; /* GPUSize32 */
  std::optional<double> endOfPassWriteIndex;       /* GPUSize32 */
};

} // namespace rnwgpu