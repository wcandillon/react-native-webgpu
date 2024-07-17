#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUTextureViewDescriptor {
  std::optional<unknown> format;         // GPUTextureFormat
  std::optional<unknown> dimension;      // GPUTextureViewDimension
  std::optional<unknown> aspect;         // GPUTextureAspect
  std::optional<double> baseMipLevel;    // GPUIntegerCoordinate
  std::optional<double> mipLevelCount;   // GPUIntegerCoordinate
  std::optional<double> baseArrayLayer;  // GPUIntegerCoordinate
  std::optional<double> arrayLayerCount; // GPUIntegerCoordinate
  std::optional<std::string> label;      // string
};

} // namespace rnwgpu