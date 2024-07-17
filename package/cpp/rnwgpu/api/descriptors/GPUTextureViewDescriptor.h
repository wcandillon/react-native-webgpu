#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUTextureViewDescriptor {
  std::optional<wgpu::TextureFormat> format; /* GPUTextureFormat */
  std::optional<wgpu::TextureViewDimension>
      dimension;                             /* GPUTextureViewDimension */
  std::optional<wgpu::TextureAspect> aspect; /* GPUTextureAspect */
  std::optional<double> baseMipLevel;        /* GPUIntegerCoordinate */
  std::optional<double> mipLevelCount;       /* GPUIntegerCoordinate */
  std::optional<double> baseArrayLayer;      /* GPUIntegerCoordinate */
  std::optional<double> arrayLayerCount;     /* GPUIntegerCoordinate */
  std::optional<std::string> label;          /* string */
};

} // namespace rnwgpu