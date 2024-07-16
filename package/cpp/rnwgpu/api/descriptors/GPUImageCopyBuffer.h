#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUBuffer.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUImageCopyBuffer {
public:
  wgpu::ImageCopyBuffer *getInstance() { return &_instance; }

  wgpu::ImageCopyBuffer _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyBuffer>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyBuffer>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyBuffer>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "buffer")) {
        auto buffer = value.getProperty(runtime, "buffer");

        if (buffer.isObject() &&
            buffer.getObject(runtime).isHostObject(runtime)) {
          result->_instance.buffer =
              buffer.getObject(runtime)
                  .asHostObject<rnwgpu::GPUBuffer>(runtime)
                  ->get();
        }

        if (buffer.isUndefined()) {
          throw std::runtime_error(
              "Property GPUImageCopyBuffer::buffer is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUImageCopyBuffer::buffer is not defined");
      }
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
                          std::shared_ptr<rnwgpu::GPUImageCopyBuffer> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
