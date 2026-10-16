#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUExtent2D.h"
#include "GPUOrigin2D.h"
#include "JSIConverter.h"
#include "VideoFrame.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// Mirror of GPUExternalTextureDescriptor from the WebGPU spec, but with our
// VideoFrame as the (only) supported source. `colorSpace` is the spec member:
// the color space of the RGB the shader samples, "srgb" (the default) or
// "display-p3"; the frame's own color space comes with the frame, and
// `sourceColorSpace` overrides the members of it the caller knows better (a
// buffer wrapped from another library carries none).
//
// `rotation` / `mirrored` are a non-spec extension: camera frames (e.g. from
// VisionCamera) arrive in the sensor's native orientation, which differs
// between iOS (CVPixelBuffer) and Android (AHardwareBuffer). Dawn's
// ExternalTextureDescriptor can bake a rotation + horizontal mirror into the
// sampling transform, so the shader sees an upright frame without any extra
// passes. `rotation` is in degrees and must be one of 0 / 90 / 180 / 270.
// `cropOrigin` / `cropSize` select the part of the frame the external texture
// shows (Dawn's cropOrigin / cropSize), the whole frame by default.
//
// createNativeBufferExternalTexture() takes the same descriptor without a
// source: the frame is its first argument.
struct GPUExternalTextureDescriptor {
  std::shared_ptr<VideoFrame> source;
  std::optional<std::string> label;
  std::optional<std::string> colorSpace;
  std::optional<VideoFrameColorSpace> sourceColorSpace;
  std::optional<double> rotation;
  std::optional<bool> mirrored;
  std::optional<std::shared_ptr<GPUOrigin2D>> cropOrigin;
  std::optional<std::shared_ptr<GPUExtent2D>> cropSize;
};

namespace external_texture_color_space {

// The JS names of the members of a source color space, the Dawn enums
// spelled the way the frame producers report them (see
// NativeVideoFrameColorSpace in src/types.ts).
inline wgpu::ColorSpaceYCbCrMatrixDawn parseMatrix(const std::string &name) {
  if (name == "identity") {
    return wgpu::ColorSpaceYCbCrMatrixDawn::Identity;
  }
  if (name == "rec601") {
    return wgpu::ColorSpaceYCbCrMatrixDawn::Rec601;
  }
  if (name == "rec709") {
    return wgpu::ColorSpaceYCbCrMatrixDawn::Rec709;
  }
  if (name == "rec2020") {
    return wgpu::ColorSpaceYCbCrMatrixDawn::Rec2020;
  }
  throw std::runtime_error("sourceColorSpace.matrix: unknown value '" + name +
                           "' (expected 'identity', 'rec601', 'rec709' or "
                           "'rec2020')");
}

inline wgpu::ColorSpaceYCbCrRangeDawn parseRange(const std::string &name) {
  if (name == "identity") {
    return wgpu::ColorSpaceYCbCrRangeDawn::Identity;
  }
  if (name == "narrow") {
    return wgpu::ColorSpaceYCbCrRangeDawn::Narrow;
  }
  if (name == "full") {
    return wgpu::ColorSpaceYCbCrRangeDawn::Full;
  }
  throw std::runtime_error("sourceColorSpace.range: unknown value '" + name +
                           "' (expected 'identity', 'narrow' or 'full')");
}

inline wgpu::ColorSpacePrimariesDawn parsePrimaries(const std::string &name) {
  if (name == "srgb") {
    return wgpu::ColorSpacePrimariesDawn::SRGB;
  }
  if (name == "rec709") {
    return wgpu::ColorSpacePrimariesDawn::Rec709;
  }
  if (name == "rec601") {
    return wgpu::ColorSpacePrimariesDawn::Rec601;
  }
  if (name == "rec2020") {
    return wgpu::ColorSpacePrimariesDawn::Rec2020;
  }
  if (name == "display-p3") {
    return wgpu::ColorSpacePrimariesDawn::DisplayP3;
  }
  throw std::runtime_error("sourceColorSpace.primaries: unknown value '" +
                           name +
                           "' (expected 'srgb', 'rec709', 'rec601', "
                           "'rec2020' or 'display-p3')");
}

inline wgpu::ColorSpaceTransferDawn parseTransfer(const std::string &name) {
  if (name == "identity") {
    return wgpu::ColorSpaceTransferDawn::Identity;
  }
  if (name == "srgb") {
    return wgpu::ColorSpaceTransferDawn::SRGB;
  }
  if (name == "display-p3") {
    return wgpu::ColorSpaceTransferDawn::DisplayP3;
  }
  if (name == "smpte-170m") {
    return wgpu::ColorSpaceTransferDawn::SMPTE_170M;
  }
  if (name == "hlg") {
    return wgpu::ColorSpaceTransferDawn::HLG;
  }
  if (name == "pq") {
    return wgpu::ColorSpaceTransferDawn::PQ;
  }
  if (name == "bt-1886") {
    return wgpu::ColorSpaceTransferDawn::BT_1886;
  }
  throw std::runtime_error("sourceColorSpace.transfer: unknown value '" + name +
                           "' (expected 'identity', 'srgb', 'display-p3', "
                           "'smpte-170m', 'hlg', 'pq' or 'bt-1886')");
}

inline VideoFrameColorSpace parse(jsi::Runtime &runtime,
                                  const jsi::Object &value) {
  VideoFrameColorSpace colorSpace;
  auto member = [&](const char *name) -> std::optional<std::string> {
    if (!value.hasProperty(runtime, name)) {
      return std::nullopt;
    }
    auto prop = value.getProperty(runtime, name);
    if (!prop.isString()) {
      return std::nullopt;
    }
    return prop.asString(runtime).utf8(runtime);
  };
  if (auto matrix = member("matrix")) {
    colorSpace.matrix = parseMatrix(*matrix);
  }
  if (auto range = member("range")) {
    colorSpace.range = parseRange(*range);
  }
  if (auto primaries = member("primaries")) {
    colorSpace.primaries = parsePrimaries(*primaries);
  }
  if (auto transfer = member("transfer")) {
    colorSpace.transfer = parseTransfer(*transfer);
  }
  return colorSpace;
}

} // namespace external_texture_color_space

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
      if (value.hasProperty(runtime, "colorSpace")) {
        auto prop = value.getProperty(runtime, "colorSpace");
        if (prop.isString()) {
          result->colorSpace = prop.asString(runtime).utf8(runtime);
        }
      }
      if (value.hasProperty(runtime, "sourceColorSpace")) {
        auto prop = value.getProperty(runtime, "sourceColorSpace");
        if (prop.isObject()) {
          result->sourceColorSpace = external_texture_color_space::parse(
              runtime, prop.getObject(runtime));
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
      if (value.hasProperty(runtime, "cropOrigin")) {
        auto prop = value.getProperty(runtime, "cropOrigin");
        if (prop.isObject()) {
          result->cropOrigin =
              JSIConverter<std::shared_ptr<GPUOrigin2D>>::fromJSI(runtime, prop,
                                                                  false);
        }
      }
      if (value.hasProperty(runtime, "cropSize")) {
        auto prop = value.getProperty(runtime, "cropSize");
        if (prop.isObject()) {
          result->cropSize =
              JSIConverter<std::shared_ptr<GPUExtent2D>>::fromJSI(runtime, prop,
                                                                  false);
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
