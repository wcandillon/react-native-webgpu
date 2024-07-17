#pragma once

#include "webgpu_cpp.h"
#include <optional>

namespace rnwgpu {

struct GPUImageCopyTextureTagged {
  std::optional<wgpu::definedColorSpace> colorSpace; /* PredefinedColorSpace */
  std::optional<bool> premultipliedAlpha;            /* boolean */
  unknown texture;                                   /* GPUTexture */
  std::optional<double> mipLevel;                    /* GPUIntegerCoordinate */
  std::optional<unknown> origin;                     /* GPUOrigin3D */
  std::optional<wgpu::TextureAspect> aspect;         /* GPUTextureAspect */
};

} // namespace rnwgpu