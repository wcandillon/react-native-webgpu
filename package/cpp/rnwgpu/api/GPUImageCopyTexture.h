#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyTexture {
public:
  wgpu::ImageCopyTexture *getInstance() { return &_instance; }

  wgpu::ImageCopyTexture _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTexture>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTexture>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyTexture>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "texture")) {
        auto texture = value.getProperty(runtime, "texture");

        else if (texture.isUndefined()) {
          throw std::runtime_error(
              "Property GPUImageCopyTexture::texture is required");
        }
      }
      if (value.hasProperty(runtime, "mipLevel")) {
        auto mipLevel = value.getProperty(runtime, "mipLevel");

        if (value.hasProperty(runtime, "mipLevel")) {
          result->_instance.mipLevel = mipLevel.getNumber();
        }
      }
      if (value.hasProperty(runtime, "origin")) {
        auto origin = value.getProperty(runtime, "origin");
      }
      if (value.hasProperty(runtime, "aspect")) {
        auto aspect = value.getProperty(runtime, "aspect");
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
