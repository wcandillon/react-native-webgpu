#pragma once

#include "webgpu/webgpu_cpp.h"

#include "GPUBlendComponent.h"

namespace rnwgpu {

struct GPUBlendState {
  std::shared_ptr<GPUBlendComponent> color; // GPUBlendComponent
  std::shared_ptr<GPUBlendComponent> alpha; // GPUBlendComponent
};

} // namespace rnwgpu