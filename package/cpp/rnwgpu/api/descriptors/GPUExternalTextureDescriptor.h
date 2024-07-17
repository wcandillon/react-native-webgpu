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

bool conv(wgpu::ExternalTextureDescriptor &out,
          const GPUExternalTextureDescriptor &in) {
  return conv(out.source, in.source) && conv(out.colorSpace, in.colorSpace) &&
         conv(out.label, in.label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUExternalTextureDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "source")) {
        auto prop = value.getProperty(runtime, "source");
        result->source = JSIConverter<
            std::variant<std::shared_ptr<HTMLVideoElement>,
                         std::shared_ptr<VideoFrame>>>::fromJSI(runtime, prop,
                                                                false);
      }
      if (value.hasProperty(runtime, "colorSpace")) {
        auto prop = value.getProperty(runtime, "colorSpace");
        result->colorSpace =
            JSIConverter<std::optional<wgpu::definedColorSpace>>::fromJSI(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter<std::optional<std::string>>::fromJSI(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUExternalTextureDescriptor> arg) {
    throw std::runtime_error("Invalid GPUExternalTextureDescriptor::toJSI()");
  }
};
} // namespace margelo