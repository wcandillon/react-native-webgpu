#pragma once

#include <optional>
#include <string>
#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "HTMLVideoElement.h"
#include "VideoFrame.h"

namespace rnwgpu {

struct GPUExternalTextureDescriptor {
  std::variant<std::shared_ptr<HTMLVideoElement>, std::shared_ptr<VideoFrame>>
      source; // | HTMLVideoElement | VideoFrame
  std::optional<wgpu::definedColorSpace> colorSpace; // PredefinedColorSpace
  std::optional<std::string> label;                  // string
};

} // namespace rnwgpu