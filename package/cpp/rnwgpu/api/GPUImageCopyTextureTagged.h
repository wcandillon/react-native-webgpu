#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyTextureTagged {
public:
  wgpu::ImageCopyTextureTagged *getInstance() { return &_instance; }

private:
  wgpu::ImageCopyTextureTagged _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUImageCopyTextureTagged>();
    if (value.hasProperty(runtime, "colorSpace")) {
      auto colorSpace = value.getProperty(runtime, "colorSpace");
      if (colorSpace.isNumber()) {
        result->_instance.colorSpace = colorSpace.getNumber();
      } else if (colorSpace.isNull() || colorSpace.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageCopyTextureTagged::colorSpace is required");
      }
    }
    if (value.hasProperty(runtime, "premultipliedAlpha")) {
      auto premultipliedAlpha =
          value.getProperty(runtime, "premultipliedAlpha");
      if (premultipliedAlpha.isNumber()) {
        result->_instance.premultipliedAlpha = premultipliedAlpha.getNumber();
      } else if (premultipliedAlpha.isNull() ||
                 premultipliedAlpha.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageCopyTextureTagged::premultipliedAlpha is "
            "required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
