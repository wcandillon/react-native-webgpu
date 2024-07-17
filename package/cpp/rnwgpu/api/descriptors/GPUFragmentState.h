#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUColorTargetState.h"
#include "GPUShaderModule.h"

namespace rnwgpu {

struct GPUFragmentState {
  std::vector<
      std::variant<std::nullptr_t, std::shared_ptr<GPUColorTargetState>>>
      targets; // Iterable<GPUColorTargetState | null>
  std::shared_ptr<GPUShaderModule> module; // GPUShaderModule
  std::optional<std::string> entryPoint;   // string
  std::optional<std::map<std::string, double>>
      constants; // Record< string, GPUPipelineConstantValue >
};

} // namespace rnwgpu