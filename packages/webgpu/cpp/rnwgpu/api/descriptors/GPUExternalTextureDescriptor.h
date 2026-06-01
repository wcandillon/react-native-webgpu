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
//
// `rotation` / `mirrored` are a non-spec extension: camera frames (e.g. from
// VisionCamera) arrive in the sensor's native orientation, which differs
// between iOS (CVPixelBuffer) and Android (AHardwareBuffer). Dawn's
// ExternalTextureDescriptor can bake a rotation + horizontal mirror into the
// sampling transform, so the shader sees an upright frame without any extra
// passes. `rotation` is in degrees and must be one of 0 / 90 / 180 / 270.
struct GPUExternalTextureDescriptor {
  std::shared_ptr<VideoFrame> source;
  std::optional<std::string> label;
  std::optional<double> rotation;
  std::optional<bool> mirrored;
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
      if (value.hasProperty(runtime, "rotation")) {
        auto prop = value.getProperty(runtime, "rotation");
        if (prop.isNumber()) {
          result->rotation = prop.asNumber();
        }
      }
      if (value.hasProperty(runtime, "mirrored")) {
        auto prop = value.getProperty(runtime, "mirrored");
        if (prop.isBool()) {
          result->mirrored = prop.getBool();
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
