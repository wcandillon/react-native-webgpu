#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyExternalImage {
public:
  wgpu::ImageCopyExternalImage *getInstance() { return &_instance; }

  wgpu::ImageCopyExternalImage _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyExternalImage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyExternalImage>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "source")) {
        auto source = value.getProperty(runtime, "source");

        if (source.isUndefined()) {
          throw std::runtime_error(
              "Property GPUImageCopyExternalImage::source is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUImageCopyExternalImage::source is not defined");
      }
      if (value.hasProperty(runtime, "origin")) {
        auto origin = value.getProperty(runtime, "origin");
      }
      if (value.hasProperty(runtime, "flipY")) {
        auto flipY = value.getProperty(runtime, "flipY");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for
    // GPUImageCopyExternalImage");
    //}
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
