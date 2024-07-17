#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPUExternalTextureDescriptor {
  unknown source;                                    /* | HTMLVideoElement
                                       | VideoFrame */
  std::optional<wgpu::definedColorSpace> colorSpace; /* PredefinedColorSpace */
  std::optional<std::string> label;                  /* string */
};

} // namespace rnwgpu