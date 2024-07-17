#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPUSamplerDescriptor {
  std::optional<wgpu::> addressModeU;  /* GPUAddressMode */
  std::optional<wgpu::> addressModeV;  /* GPUAddressMode */
  std::optional<wgpu::> addressModeW;  /* GPUAddressMode */
  std::optional<wgpu::> magFilter;     /* GPUFilterMode */
  std::optional<wgpu::> minFilter;     /* GPUFilterMode */
  std::optional<wgpu::> mipmapFilter;  /* GPUMipmapFilterMode */
  std::optional<double> lodMinClamp;   /* number */
  std::optional<double> lodMaxClamp;   /* number */
  std::optional<wgpu::> compare;       /* GPUCompareFunction */
  std::optional<double> maxAnisotropy; /* number */
  std::optional<std::string> label;    /* string */
};

} // namespace rnwgpu