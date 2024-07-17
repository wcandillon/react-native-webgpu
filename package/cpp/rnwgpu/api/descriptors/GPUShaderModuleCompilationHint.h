#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUShaderModuleCompilationHint {
  std::string entryPoint;        // string
  std::optional<unknown> layout; // | GPUPipelineLayout     | GPUAutoLayoutMode
};

} // namespace rnwgpu