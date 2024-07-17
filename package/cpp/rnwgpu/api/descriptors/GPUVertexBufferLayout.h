#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUVertexBufferLayout {
  double arrayStride;                           /* GPUSize64 */
  std::optional<wgpu::VertexStepMode> stepMode; /* GPUVertexStepMode */
  unknown attributes; /* Iterable<GPUVertexAttribute> */
};

} // namespace rnwgpu