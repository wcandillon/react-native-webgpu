#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUSamplerDescriptor {
  std::optional<wgpu::AddressMode> addressModeU;      // GPUAddressMode
  std::optional<wgpu::AddressMode> addressModeV;      // GPUAddressMode
  std::optional<wgpu::AddressMode> addressModeW;      // GPUAddressMode
  std::optional<wgpu::FilterMode> magFilter;          // GPUFilterMode
  std::optional<wgpu::FilterMode> minFilter;          // GPUFilterMode
  std::optional<wgpu::MipmapFilterMode> mipmapFilter; // GPUMipmapFilterMode
  std::optional<double> lodMinClamp;                  // number
  std::optional<double> lodMaxClamp;                  // number
  std::optional<wgpu::CompareFunction> compare;       // GPUCompareFunction
  std::optional<double> maxAnisotropy;                // number
  std::optional<std::string> label;                   // string
};

} // namespace rnwgpu