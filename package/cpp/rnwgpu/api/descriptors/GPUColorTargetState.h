#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUBlendState.h"

namespace rnwgpu {

struct GPUColorTargetState {
  wgpu::TextureFormat format;                          // GPUTextureFormat
  std::optional<std::shared_ptr<GPUBlendState>> blend; // GPUBlendState
  std::optional<double> writeMask;                     // GPUColorWriteFlags
};

} // namespace rnwgpu