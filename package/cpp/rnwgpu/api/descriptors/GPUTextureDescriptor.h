#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUExtent3DDictStrict.h"

namespace rnwgpu {

struct GPUTextureDescriptor {
  std::variant<std::vector<double>, std::shared_ptr<GPUExtent3DDictStrict>>
      size;                                        // GPUExtent3DStrict
  std::optional<double> mipLevelCount;             // GPUIntegerCoordinate
  std::optional<double> sampleCount;               // GPUSize32
  std::optional<wgpu::TextureDimension> dimension; // GPUTextureDimension
  wgpu::TextureFormat format;                      // GPUTextureFormat
  double usage;                                    // GPUTextureUsageFlags
  std::optional<std::vector<wgpu::TextureFormat>>
      viewFormats;                  // Iterable<GPUTextureFormat>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu