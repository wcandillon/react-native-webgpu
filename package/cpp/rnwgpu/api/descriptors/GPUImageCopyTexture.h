#pragma once

#include <optional>

namespace rnwgpu {

struct GPUImageCopyTexture {
  unknown texture;                // GPUTexture
  std::optional<double> mipLevel; // GPUIntegerCoordinate
  std::optional<unknown> origin;  // GPUOrigin3D
  std::optional<unknown> aspect;  // GPUTextureAspect
};

} // namespace rnwgpu