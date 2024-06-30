#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyExternalImage {
public:
  wgpu::ImageCopyExternalImage *getInstance() { return &_instance; }

private:
  wgpu::ImageCopyExternalImage _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUImageCopyExternalImage>();
    if (value.hasProperty(runtime, "source")) {
      auto source = value.getProperty(runtime, "source");
      if (source.isNumber()) {
        result->_instance.source = source.getNumber();
      }
    }
    if (value.hasProperty(runtime, "origin")) {
      auto origin = value.getProperty(runtime, "origin");
      if (origin.isNumber()) {
        result->_instance.origin = origin.getNumber();
      } else if (origin.isNull() || origin.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageCopyExternalImage::origin is required");
      }
    }
    if (value.hasProperty(runtime, "flipY")) {
      auto flipY = value.getProperty(runtime, "flipY");
      if (flipY.isNumber()) {
        result->_instance.flipY = flipY.getNumber();
      } else if (flipY.isNull() || flipY.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageCopyExternalImage::flipY is required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUImageCopyExternalImage> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
