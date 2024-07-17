#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUSamplerDescriptor {
  std::optional<unknown> addressModeU; // GPUAddressMode
  std::optional<unknown> addressModeV; // GPUAddressMode
  std::optional<unknown> addressModeW; // GPUAddressMode
  std::optional<unknown> magFilter;    // GPUFilterMode
  std::optional<unknown> minFilter;    // GPUFilterMode
  std::optional<unknown> mipmapFilter; // GPUMipmapFilterMode
  std::optional<double> lodMinClamp;   // number
  std::optional<double> lodMaxClamp;   // number
  std::optional<unknown> compare;      // GPUCompareFunction
  std::optional<double> maxAnisotropy; // number
  std::optional<std::string> label;    // string
};

} // namespace rnwgpu