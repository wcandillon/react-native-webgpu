#pragma once

#include <string>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUProgrammableStage.h"
#include "GPUPipelineLayout.h"

namespace rnwgpu {

struct GPUComputePipelineDescriptor {
  std::shared_ptr<GPUProgrammableStage> compute; /* GPUProgrammableStage */
  std::variant<std::null_ptr, std::shared_ptr<GPUPipelineLayout>> layout; /* | GPUPipelineLayout
        | GPUAutoLayoutMode */
  std::optional<std::string> label; /* string */
};

} // namespace rnwgpu