#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUVertexState {
  std::optional<unknown> buffers; /* Iterable<GPUVertexBufferLayout | null> */
  unknown module;                 /* GPUShaderModule */
  std::optional<std::string> entryPoint; /* string */
  std::optional<unknown> constants;      /* Record<
             string,
             GPUPipelineConstantValue
             > */
};

} // namespace rnwgpu