#pragma once

#include <optional>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUTextureBindingLayout {
  std::optional<wgpu::> sampleType;    /* GPUTextureSampleType */
  std::optional<wgpu::> viewDimension; /* GPUTextureViewDimension */
  std::optional<bool> multisampled;    /* boolean */
};

} // namespace rnwgpu