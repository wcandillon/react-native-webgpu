#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUOrigin3DDict.h"
#include "GPUTexture.h"

namespace rnwgpu {

struct GPUImageCopyTextureTagged {
  std::optional<wgpu::definedColorSpace> colorSpace; // PredefinedColorSpace
  std::optional<bool> premultipliedAlpha;            // boolean
  std::shared_ptr<GPUTexture> texture;               // GPUTexture
  std::optional<double> mipLevel;                    // GPUIntegerCoordinate
  std::optional<
      std::variant<std::vector<double>, std::shared_ptr<GPUOrigin3DDict>>>
      origin;                                // GPUOrigin3D
  std::optional<wgpu::TextureAspect> aspect; // GPUTextureAspect
};

} // namespace rnwgpu