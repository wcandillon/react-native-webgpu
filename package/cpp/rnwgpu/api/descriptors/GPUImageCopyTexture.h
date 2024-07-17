#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUImageCopyTexture {
  unknown texture;                           /* GPUTexture */
  std::optional<double> mipLevel;            /* GPUIntegerCoordinate */
  std::optional<unknown> origin;             /* GPUOrigin3D */
  std::optional<wgpu::TextureAspect> aspect; /* GPUTextureAspect */
};

} // namespace rnwgpu