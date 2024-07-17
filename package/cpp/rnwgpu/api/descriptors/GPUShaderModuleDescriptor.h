#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUShaderModuleDescriptor {
  std::string code; /* string */
  std::optional<unknown>
      compilationHints;             /* Array<GPUShaderModuleCompilationHint> */
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu