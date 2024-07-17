#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUTextureViewDescriptor {
  std::optional<wgpu::TextureFormat> format; // GPUTextureFormat
  std::optional<wgpu::TextureViewDimension>
      dimension;                             // GPUTextureViewDimension
  std::optional<wgpu::TextureAspect> aspect; // GPUTextureAspect
  std::optional<double> baseMipLevel;        // GPUIntegerCoordinate
  std::optional<double> mipLevelCount;       // GPUIntegerCoordinate
  std::optional<double> baseArrayLayer;      // GPUIntegerCoordinate
  std::optional<double> arrayLayerCount;     // GPUIntegerCoordinate
  std::optional<std::string> label;          // string
};

} // namespace rnwgpu