#pragma once

#include <optional>

namespace rnwgpu {

struct GPUImageCopyTextureTagged {
  std::optional<unknown> colorSpace;      // PredefinedColorSpace
  std::optional<bool> premultipliedAlpha; // boolean
  unknown texture;                        // GPUTexture
  std::optional<double> mipLevel;         // GPUIntegerCoordinate
  std::optional<unknown> origin;          // GPUOrigin3D
  std::optional<unknown> aspect;          // GPUTextureAspect
};

} // namespace rnwgpu