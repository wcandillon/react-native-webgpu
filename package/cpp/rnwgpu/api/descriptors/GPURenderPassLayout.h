#pragma once

#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPURenderPassLayout {
  std::vector<std::variant<wgpu::TextureFormat, std::nullptr_t>>
      colorFormats; // Iterable<GPUTextureFormat | null>
  std::optional<wgpu::TextureFormat> depthStencilFormat; // GPUTextureFormat
  std::optional<double> sampleCount;                     // GPUSize32
  std::optional<std::string> label;                      // string
};

} // namespace rnwgpu