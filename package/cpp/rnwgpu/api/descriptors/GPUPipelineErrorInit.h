#pragma once

#include "webgpu_cpp.h"

namespace rnwgpu {

struct GPUPipelineErrorInit {
  wgpu::PipelineErrorReason reason; /* GPUPipelineErrorReason */
};

} // namespace rnwgpu