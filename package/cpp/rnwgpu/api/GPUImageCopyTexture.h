#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyTexture {
public:
  wgpu::ImageCopyTexture *getInstance() { return &_instance; }

  wgpu::ImageCopyTexture _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTexture>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTexture>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyTexture>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "texture")) {
        auto texture = value.getProperty(runtime, "texture");

        if (texture.isUndefined()) {
          throw std::runtime_error(
              "Property GPUImageCopyTexture::texture is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUImageCopyTexture::texture is not defined");
      }
      if (value.hasProperty(runtime, "mipLevel")) {
        auto mipLevel = value.getProperty(runtime, "mipLevel");

        if (mipLevel.isNumber()) {
          result->_instance.mipLevel =
              static_cast<wgpu::IntegerCoordinate>(mipLevel.getNumber());
        }
      }
      if (value.hasProperty(runtime, "origin")) {
        auto origin = value.getProperty(runtime, "origin");
      }
      if (value.hasProperty(runtime, "aspect")) {
        auto aspect = value.getProperty(runtime, "aspect");
      }
    }
    rnwgpu::Logger::logToConsole("GPUImageCopyTexture::texture = %f",
                                 result->_instance.texture);
    rnwgpu::Logger::logToConsole("GPUImageCopyTexture::mipLevel = %f",
                                 result->_instance.mipLevel);
    rnwgpu::Logger::logToConsole("GPUImageCopyTexture::origin = %f",
                                 result->_instance.origin);
    rnwgpu::Logger::logToConsole("GPUImageCopyTexture::aspect = %f",
                                 result->_instance.aspect);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageCopyTexture> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
