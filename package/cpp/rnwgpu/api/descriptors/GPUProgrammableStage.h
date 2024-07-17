#pragma once

#include <map>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUShaderModule.h"

namespace rnwgpu {

struct GPUProgrammableStage {
  std::shared_ptr<GPUShaderModule> module; // GPUShaderModule
  std::optional<std::string> entryPoint;   // string
  std::optional<std::map<std::string, double>>
      constants; // Record< string, GPUPipelineConstantValue >
};

} // namespace rnwgpu