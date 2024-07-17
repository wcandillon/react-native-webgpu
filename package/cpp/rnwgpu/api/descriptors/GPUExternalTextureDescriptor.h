#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPUExternalTextureDescriptor {
  unknown source;                    // | HTMLVideoElement     | VideoFrame
  std::optional<unknown> colorSpace; // PredefinedColorSpace
  std::optional<std::string> label;  // string
};

} // namespace rnwgpu