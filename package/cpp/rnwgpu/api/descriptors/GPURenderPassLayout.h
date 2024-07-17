#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPURenderPassLayout {
  unknown colorFormats; /* Iterable<GPUTextureFormat | null> */
  std::optional<wgpu::TextureFormat> depthStencilFormat; /* GPUTextureFormat */
  std::optional<double> sampleCount;                     /* GPUSize32 */
  std::optional<std::string> label;                      /* string */
};

} // namespace rnwgpu