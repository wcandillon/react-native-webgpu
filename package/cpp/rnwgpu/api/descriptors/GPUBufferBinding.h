#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUBuffer.h"

namespace rnwgpu {

struct GPUBufferBinding {
  std::shared_ptr<GPUBuffer> buffer; // GPUBuffer
  std::optional<double> offset;      // GPUSize64
  std::optional<double> size;        // GPUSize64
};

} // namespace rnwgpu