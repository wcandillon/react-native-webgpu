#pragma once

#include <optional>

namespace rnwgpu {

struct GPUVertexBufferLayout {
  double arrayStride;              // GPUSize64
  std::optional<unknown> stepMode; // GPUVertexStepMode
  unknown attributes;              // Iterable<GPUVertexAttribute>
};

} // namespace rnwgpu