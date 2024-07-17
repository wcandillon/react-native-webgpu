#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUTextureDescriptor {
  unknown size;                                    /* GPUExtent3DStrict */
  std::optional<double> mipLevelCount;             /* GPUIntegerCoordinate */
  std::optional<double> sampleCount;               /* GPUSize32 */
  std::optional<wgpu::TextureDimension> dimension; /* GPUTextureDimension */
  wgpu::TextureFormat format;                      /* GPUTextureFormat */
  double usage;                                    /* GPUTextureUsageFlags */
  std::optional<unknown> viewFormats; /* Iterable<GPUTextureFormat> */
  std::optional<std::string> label;   /* string */
};

} // namespace rnwgpu