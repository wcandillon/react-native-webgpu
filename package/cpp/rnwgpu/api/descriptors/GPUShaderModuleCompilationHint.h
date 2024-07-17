#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUPipelineLayout.h"

namespace rnwgpu {

struct GPUShaderModuleCompilationHint {
  std::string entryPoint; // string
  std::optional<std::variant<std::null_ptr, std::shared_ptr<GPUPipelineLayout>>>
      layout; // | GPUPipelineLayout | GPUAutoLayoutMode
};

} // namespace rnwgpu