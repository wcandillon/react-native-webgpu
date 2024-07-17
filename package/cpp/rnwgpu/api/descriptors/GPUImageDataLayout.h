#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUImageDataLayout {
  std::optional<double> offset;       /* GPUSize64 */
  std::optional<double> bytesPerRow;  /* GPUSize32 */
  std::optional<double> rowsPerImage; /* GPUSize32 */
};

} // namespace rnwgpu