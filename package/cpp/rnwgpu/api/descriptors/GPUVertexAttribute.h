#pragma once

#include "webgpu_cpp.h"

namespace rnwgpu {

struct GPUVertexAttribute {
  wgpu::VertexFormat format; /* GPUVertexFormat */
  double offset;             /* GPUSize64 */
  double shaderLocation;     /* GPUIndex32 */
};

} // namespace rnwgpu