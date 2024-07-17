#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUShaderModuleCompilationHint.h"

namespace rnwgpu {

struct GPUShaderModuleDescriptor {
  std::string code; // string
  std::optional<std::vector<std::shared_ptr<GPUShaderModuleCompilationHint>>>
      compilationHints;             // Array<GPUShaderModuleCompilationHint>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu