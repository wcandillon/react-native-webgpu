#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBufferDescriptor {
public:
  wgpu::BufferDescriptor *getInstance() { return &_instance; }

private:
  wgpu::BufferDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBufferDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUBufferDescriptor>();
    if (value.hasProperty(runtime, "size")) {
      auto size = value.getProperty(runtime, "size");
      if (size.isNumber()) {
        result->_instance.size = size.getNumber();
      }
    }
    if (value.hasProperty(runtime, "usage")) {
      auto usage = value.getProperty(runtime, "usage");
      if (usage.isNumber()) {
        result->_instance.usage = usage.getNumber();
      }
    }
    if (value.hasProperty(runtime, "mappedAtCreation")) {
      auto mappedAtCreation = value.getProperty(runtime, "mappedAtCreation");
      if (mappedAtCreation.isNumber()) {
        result->_instance.mappedAtCreation = mappedAtCreation.getNumber();
      } else if (mappedAtCreation.isNull() || mappedAtCreation.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBufferDescriptor::mappedAtCreation is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
