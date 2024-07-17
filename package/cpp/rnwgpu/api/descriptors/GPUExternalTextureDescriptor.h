#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "HTMLVideoElement.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"
#include "VideoFrame.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUExternalTextureDescriptor {
  std::variant<std::shared_ptr<HTMLVideoElement>, std::shared_ptr<VideoFrame>>
      source; // | HTMLVideoElement | VideoFrame
  std::optional<wgpu::definedColorSpace> colorSpace; // PredefinedColorSpace
  std::optional<std::string> label;                  // string
};

} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUExternalTextureDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo