#pragma once

#include <memory>

#include "ArrayBuffer.h"

namespace rnwgpu {

  struct ImageData {
    std::shared_ptr<ArrayBuffer> data;
    size_t width;
    size_t height;
  };

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::ImageData>> {
  static std::shared_ptr<rnwgpu::ImageData>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::ImageData>();
    if (!outOfBounds && arg.isObject()) {
      auto obj = arg.getObject(runtime);
      if (obj.hasProperty(runtime, "data")) {
        auto prop = obj.getProperty(runtime, "data");
        result->data = JSIConverter<std::shared_ptr<ArrayBuffer>>::fromJSI(runtime, prop, false);
      }      
      if (obj.hasProperty(runtime, "width")) {
        auto prop = obj.getProperty(runtime, "width");
        result->width = prop.getNumber();
      }
      if (obj.hasProperty(runtime, "height")) {
        auto prop = obj.getProperty(runtime, "height");
        result->height = prop.getNumber();
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::ImageData> arg) {
    throw std::runtime_error("Invalid ImageData::toJSI()");
  }
};

} // namespace margelo