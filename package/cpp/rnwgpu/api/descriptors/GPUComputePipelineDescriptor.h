#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUComputePipelineDescriptor {
  unknown compute; // GPUProgrammableStage
  unknown layout;  // | GPUPipelineLayout         | GPUAutoLayoutMode
  std::optional<std::string> label; // string
};

} // namespace rnwgpu