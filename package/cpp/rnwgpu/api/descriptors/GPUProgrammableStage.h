#pragma once

#include <string>
#include <optional>
#include <map>

#include "webgpu/webgpu_cpp.h"

#include "GPUShaderModule.h"

namespace rnwgpu {

struct GPUProgrammableStage {
  std::shared_ptr<GPUShaderModule> module; /* GPUShaderModule */
  std::optional<std::string> entryPoint; /* string */
  std::optional<std::map<undefined, undefined>> constants; /* Record<
    string,
    GPUPipelineConstantValue
  > */
};

} // namespace rnwgpu