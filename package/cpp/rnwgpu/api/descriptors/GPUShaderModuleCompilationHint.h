#pragma once

#include <optional>
#include <string>
#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "GPUPipelineLayout.h"

namespace rnwgpu {

struct GPUShaderModuleCompilationHint {
  std::string entryPoint; /* string */
  std::optional<std::variant<std::shared_ptr<GPUPipelineLayout>, unknown>>
      layout; /* | GPUPipelineLayout
| GPUAutoLayoutMode */
};

} // namespace rnwgpu