#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUShaderModuleDescriptor {
  std::string code; // string
  std::optional<unknown>
      compilationHints;             // Array<GPUShaderModuleCompilationHint>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu