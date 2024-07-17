#pragma once

#include <optional>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUVertexAttribute.h"

namespace rnwgpu {

struct GPUVertexBufferLayout {
  double arrayStride;             /* GPUSize64 */
  std::optional<wgpu::> stepMode; /* GPUVertexStepMode */
  std::vector<std::shared_ptr<GPUVertexAttribute>>
      attributes; /* Iterable<GPUVertexAttribute> */
};

} // namespace rnwgpu