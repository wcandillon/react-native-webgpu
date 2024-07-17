#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUTextureDescriptor {
  unknown size;                        // GPUExtent3DStrict
  std::optional<double> mipLevelCount; // GPUIntegerCoordinate
  std::optional<double> sampleCount;   // GPUSize32
  std::optional<unknown> dimension;    // GPUTextureDimension
  unknown format;                      // GPUTextureFormat
  double usage;                        // GPUTextureUsageFlags
  std::optional<unknown> viewFormats;  // Iterable<GPUTextureFormat>
  std::optional<std::string> label;    // string
};

} // namespace rnwgpu