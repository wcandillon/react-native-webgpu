#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyTexture {
public:
  wgpu::ImageCopyTexture *getInstance() { return &_instance; }

private:
  wgpu::ImageCopyTexture _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTexture>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTexture>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUImageCopyTexture>();
    if (value.hasProperty(runtime, "texture")) {
      auto texture = value.getProperty(runtime, "texture");
      if (texture.isNumber()) {
        result->_instance.texture = texture.getNumber();
      }
    }
    if (value.hasProperty(runtime, "mipLevel")) {
      auto mipLevel = value.getProperty(runtime, "mipLevel");
      if (mipLevel.isNumber()) {
        result->_instance.mipLevel = mipLevel.getNumber();
      } else if (mipLevel.isNull() || mipLevel.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageCopyTexture::mipLevel is required");
      }
    }
    if (value.hasProperty(runtime, "origin")) {
      auto origin = value.getProperty(runtime, "origin");
      if (origin.isNumber()) {
        result->_instance.origin = origin.getNumber();
      } else if (origin.isNull() || origin.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageCopyTexture::origin is required");
      }
    }
    if (value.hasProperty(runtime, "aspect")) {
      auto aspect = value.getProperty(runtime, "aspect");
      if (aspect.isNumber()) {
        result->_instance.aspect = aspect.getNumber();
      } else if (aspect.isNull() || aspect.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageCopyTexture::aspect is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageCopyTexture> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
