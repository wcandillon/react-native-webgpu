#pragma once

namespace rnwgpu {

struct GPUVertexAttribute {
  unknown format;        // GPUVertexFormat
  double offset;         // GPUSize64
  double shaderLocation; // GPUIndex32
};

} // namespace rnwgpu