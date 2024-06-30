#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBufferBinding {
public:
  wgpu::BufferBinding *getInstance() { return &_instance; }

private:
  wgpu::BufferBinding _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferBinding>> {
  static std::shared_ptr<rnwgpu::GPUBufferBinding>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUBufferBinding>();
    if (value.hasProperty(runtime, "buffer")) {
      auto buffer = value.getProperty(runtime, "buffer");
      if (buffer.isNumber()) {
        result->_instance.buffer = buffer.getNumber();
      }
    }
    if (value.hasProperty(runtime, "offset")) {
      auto offset = value.getProperty(runtime, "offset");
      if (offset.isNumber()) {
        result->_instance.offset = offset.getNumber();
      } else if (offset.isNull() || offset.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBufferBinding::offset is required");
      }
    }
    if (value.hasProperty(runtime, "size")) {
      auto size = value.getProperty(runtime, "size");
      if (size.isNumber()) {
        result->_instance.size = size.getNumber();
      } else if (size.isNull() || size.isUndefined()) {
        throw std::runtime_error("Property GPUBufferBinding::size is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferBinding> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
