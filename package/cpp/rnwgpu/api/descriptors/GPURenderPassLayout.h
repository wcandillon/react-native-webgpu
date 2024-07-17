#pragma once

#include <optional>
#include <string>
#include <variant>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct GPURenderPassLayout {
  std::vector<std::variant<std::nullptr_t, unknown>>
      colorFormats; /* Iterable<GPUTextureFormat | null> */
  std::optional<wgpu::TextureFormat> depthStencilFormat; /* GPUTextureFormat */
  std::optional<double> sampleCount;                     /* GPUSize32 */
  std::optional<std::string> label;                      /* string */
};

} // namespace rnwgpu