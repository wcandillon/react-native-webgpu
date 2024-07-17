#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUBuffer.h"

namespace rnwgpu {

struct GPUImageCopyBuffer {
  std::shared_ptr<GPUBuffer> buffer;  /* GPUBuffer */
  std::optional<double> offset;       /* GPUSize64 */
  std::optional<double> bytesPerRow;  /* GPUSize32 */
  std::optional<double> rowsPerImage; /* GPUSize32 */
};

} // namespace rnwgpu