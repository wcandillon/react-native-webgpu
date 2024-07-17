#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUFragmentState {
  unknown targets;                       // Iterable<GPUColorTargetState | null>
  unknown module;                        // GPUShaderModule
  std::optional<std::string> entryPoint; // string
  std::optional<unknown>
      constants; // Record<         string,         GPUPipelineConstantValue >
};

} // namespace rnwgpu