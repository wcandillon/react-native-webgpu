#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUColorTargetState {
  wgpu::TextureFormat format;      /* GPUTextureFormat */
  std::optional<unknown> blend;    /* GPUBlendState */
  std::optional<double> writeMask; /* GPUColorWriteFlags */
};

} // namespace rnwgpu