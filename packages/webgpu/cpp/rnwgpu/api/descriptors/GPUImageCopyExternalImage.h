#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "JSIConverter.h"

#include "GPUOrigin2D.h"
#include "ImageBitmap.h"
#include "VideoFrame.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// GPUCopyExternalImageSourceInfo. The source is either an ImageBitmap (the
// spec's CPU upload) or, as a React Native extension, a NativeVideoFrame that
// is rendered into the destination through an external texture (see
// VideoFrameBlit). `rotation` and `mirrored` only apply to a native frame and
// mirror the extension of GPUExternalTextureDescriptor.
struct GPUImageCopyExternalImage {
  std::shared_ptr<ImageBitmap> source;
  std::shared_ptr<VideoFrame> videoFrame;
  std::optional<std::shared_ptr<GPUOrigin2D>> origin; // GPUOrigin2DStrict
  std::optional<bool> flipY;                          // boolean
  std::optional<double> rotation;
  std::optional<bool> mirrored;
};

} // namespace rnwgpu

namespace rnwgpu {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyExternalImage>();
    if (!outOfBounds && arg.isObject()) {
      auto obj = arg.getObject(runtime);
      if (obj.hasProperty(runtime, "source")) {
        auto prop = obj.getProperty(runtime, "source");
        if (prop.isObject() &&
            prop.getObject(runtime).hasNativeState<VideoFrame>(runtime)) {
          result->videoFrame =
              prop.getObject(runtime).getNativeState<VideoFrame>(runtime);
        } else {
          result->source = JSIConverter<std::shared_ptr<ImageBitmap>>::fromJSI(
              runtime, prop, false);
        }
      }
      if (obj.hasProperty(runtime, "origin")) {
        auto prop = obj.getProperty(runtime, "origin");
        result->origin = JSIConverter<std::shared_ptr<GPUOrigin2D>>::fromJSI(
            runtime, prop, false);
      }

      if (obj.hasProperty(runtime, "flipY")) {
        auto prop = obj.getProperty(runtime, "flipY");
        result->flipY = JSIConverter<bool>::fromJSI(runtime, prop, false);
      }
      if (obj.hasProperty(runtime, "rotation")) {
        auto prop = obj.getProperty(runtime, "rotation");
        if (prop.isNumber()) {
          result->rotation = prop.asNumber();
        }
      }
      if (obj.hasProperty(runtime, "mirrored")) {
        auto prop = obj.getProperty(runtime, "mirrored");
        if (prop.isBool()) {
          result->mirrored = prop.getBool();
        }
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUImageCopyExternalImage> arg) {
    throw std::runtime_error("Invalid GPUImageCopyExternalImage::toJSI()");
  }
};

} // namespace rnwgpu
