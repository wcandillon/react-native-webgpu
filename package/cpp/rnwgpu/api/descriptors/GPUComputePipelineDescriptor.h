#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUPipelineLayout.h"
#include "GPUProgrammableStage.h"

namespace rnwgpu {

struct GPUComputePipelineDescriptor {
  std::shared_ptr<GPUProgrammableStage> compute; // GPUProgrammableStage
  std::variant<std::null_ptr, std::shared_ptr<GPUPipelineLayout>>
      layout;                       // | GPUPipelineLayout | GPUAutoLayoutMode
  std::optional<std::string> label; // string
};

} // namespace rnwgpu