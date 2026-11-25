#pragma once

#include <memory>
#include <optional>
#include <stdexcept>

#include "GPUExternalTextureDescriptor.h"
#include "JSIConverter.h"
#include "PlatformContext.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// The options of createNativeBufferFromPointer(). `colorSpace` (a
// NativeBufferColorSpace, as importExternalTexture's sourceColorSpace) says
// how to read a YUV buffer that carries no color information of its own, as
// an AHardwareBuffer from a MediaCodec ImageReader. The members it sets
// replace what the buffer says.
struct CreateNativeBufferFromPointerOptions {
  std::optional<VideoFrameColorSpace> colorSpace;
};

template <>
struct JSIConverter<std::shared_ptr<CreateNativeBufferFromPointerOptions>> {
  static std::shared_ptr<CreateNativeBufferFromPointerOptions>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_shared<CreateNativeBufferFromPointerOptions>();
    if (!outOfBounds && arg.isObject()) {
      auto prop = arg.getObject(runtime).getProperty(runtime, "colorSpace");
      if (prop.isObject()) {
        result->colorSpace = external_texture_color_space::parse(
            runtime, prop.getObject(runtime));
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<CreateNativeBufferFromPointerOptions> arg) {
    throw std::runtime_error(
        "Invalid CreateNativeBufferFromPointerOptions::toJSI()");
  }
};

} // namespace rnwgpu
