#pragma once

#include <optional>
#include <string>
#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "GPUShaderModule.h"
#include "GPUVertexBufferLayout.h"

namespace rnwgpu {

struct GPUVertexState {
  std::optional<std::vector<
      std::variant<std::nullptr_t, std::shared_ptr<GPUVertexBufferLayout>>>>
      buffers; /* Iterable<GPUVertexBufferLayout | null> */
  std::shared_ptr<GPUShaderModule> module; /* GPUShaderModule */
  std::optional<std::string> entryPoint;   /* string */
  std::optional<unknown> constants;        /* Record<
               string,
               GPUPipelineConstantValue
               > */
};

} // namespace rnwgpu