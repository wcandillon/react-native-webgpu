#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageDataLayout {
public:
  wgpu::ImageDataLayout *getInstance() { return &_instance; }

  wgpu::ImageDataLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageDataLayout>> {
  static std::shared_ptr<rnwgpu::GPUImageDataLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUImageDataLayout>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "offset")) {
        auto offset = value.getProperty(runtime, "offset");

        if (value.hasProperty(runtime, "offset")) {
          result->_instance.offset = offset.getNumber();
        }
      }
      if (value.hasProperty(runtime, "bytesPerRow")) {
        auto bytesPerRow = value.getProperty(runtime, "bytesPerRow");

        if (value.hasProperty(runtime, "bytesPerRow")) {
          result->_instance.bytesPerRow = bytesPerRow.getNumber();
        }
      }
      if (value.hasProperty(runtime, "rowsPerImage")) {
        auto rowsPerImage = value.getProperty(runtime, "rowsPerImage");

        if (value.hasProperty(runtime, "rowsPerImage")) {
          result->_instance.rowsPerImage = rowsPerImage.getNumber();
        }
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
