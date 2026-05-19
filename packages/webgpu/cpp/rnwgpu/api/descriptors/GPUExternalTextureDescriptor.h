#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "JSIConverter.h"
#include "VideoFrame.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// Mirror of GPUExternalTextureDescriptor from the WebGPU spec, but with our
// VideoFrame as the (only) supported source. We don't expose colorSpace yet;
// the C++ side picks dst-sRGB and identity gamut, which is the right default
// for "render this video frame to a regular sRGB framebuffer".
struct GPUExternalTextureDescriptor {
  std::shared_ptr<VideoFrame> source;
  std::optional<std::string> label;
};

} // namespace rnwgpu

namespace rnwgpu {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_shared<rnwgpu::GPUExternalTextureDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "source")) {
        auto prop = value.getProperty(runtime, "source");
        if (!prop.isUndefined() && !prop.isNull()) {
          result->source =
              JSIConverter<std::shared_ptr<rnwgpu::VideoFrame>>::fromJSI(
                  runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        if (!prop.isUndefined()) {
          result->label = JSIConverter<std::optional<std::string>>::fromJSI(
              runtime, prop, false);
        }
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime & /*runtime*/,
        std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor> /*arg*/) {
    throw std::runtime_error("Invalid GPUExternalTextureDescriptor::toJSI()");
  }
};

} // namespace rnwgpu
