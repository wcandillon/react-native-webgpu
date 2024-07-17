#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUTextureBindingLayout {
  std::optional<wgpu::TextureSampleType> sampleType; /* GPUTextureSampleType */
  std::optional<wgpu::TextureViewDimension>
      viewDimension;                /* GPUTextureViewDimension */
  std::optional<bool> multisampled; /* boolean */
};

} // namespace rnwgpu