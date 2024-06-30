#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyExternalImage {
public:
  wgpu::ImageCopyExternalImage *getInstance() { return &_instance; }

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

      else if (source.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageCopyExternalImage::source is required");
      }
    }
    if (value.hasProperty(runtime, "origin")) {
      auto origin = value.getProperty(runtime, "origin");
    }
    if (value.hasProperty(runtime, "flipY")) {
      auto flipY = value.getProperty(runtime, "flipY");
      if (value.hasProperty(runtime, "flipY")) {
        result->_instance.flipY = flipY.getBool();
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
