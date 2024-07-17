#pragma once

#include "webgpu_cpp.h"

namespace rnwgpu {

struct GPUBlendState {
  unknown color; /* GPUBlendComponent */
  unknown alpha; /* GPUBlendComponent */
};

} // namespace rnwgpu