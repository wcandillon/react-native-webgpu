#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;
namespace m = margelo;

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
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageDataLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "offset")) {
        auto offset = value.getProperty(runtime, "offset");

        if (offset.isNumber()) {
          result->_instance.offset = offset.getNumber();
        }
      }
      if (value.hasProperty(runtime, "bytesPerRow")) {
        auto bytesPerRow = value.getProperty(runtime, "bytesPerRow");

        if (bytesPerRow.isNumber()) {
          result->_instance.bytesPerRow =
              static_cast<uint32_t>(bytesPerRow.getNumber());
        }
      }
      if (value.hasProperty(runtime, "rowsPerImage")) {
        auto rowsPerImage = value.getProperty(runtime, "rowsPerImage");

        if (rowsPerImage.isNumber()) {
          result->_instance.rowsPerImage =
              static_cast<uint32_t>(rowsPerImage.getNumber());
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
