#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageDataLayout {
public:
  wgpu::ImageDataLayout *getInstance() { return &_instance; }

private:
  wgpu::ImageDataLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageDataLayout>> {
  static std::shared_ptr<rnwgpu::GPUImageDataLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUImageDataLayout>();
    if (value.hasProperty(runtime, "offset")) {
      auto offset = value.getProperty(runtime, "offset");
      if (offset.isNumber()) {
        result->_instance.offset = offset.getNumber();
      } else if (offset.isNull() || offset.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageDataLayout::offset is required");
      }
    }
    if (value.hasProperty(runtime, "bytesPerRow")) {
      auto bytesPerRow = value.getProperty(runtime, "bytesPerRow");
      if (bytesPerRow.isNumber()) {
        result->_instance.bytesPerRow = bytesPerRow.getNumber();
      } else if (bytesPerRow.isNull() || bytesPerRow.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageDataLayout::bytesPerRow is required");
      }
    }
    if (value.hasProperty(runtime, "rowsPerImage")) {
      auto rowsPerImage = value.getProperty(runtime, "rowsPerImage");
      if (rowsPerImage.isNumber()) {
        result->_instance.rowsPerImage = rowsPerImage.getNumber();
      } else if (rowsPerImage.isNull() || rowsPerImage.isUndefined()) {
        throw std::runtime_error(
            "Property GPUImageDataLayout::rowsPerImage is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageDataLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
