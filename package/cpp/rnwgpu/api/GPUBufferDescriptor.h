#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBufferDescriptor {
public:
  wgpu::BufferDescriptor *getInstance() { return &_instance; }

  wgpu::BufferDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBufferDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUBufferDescriptor>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "size")) {
        auto size = value.getProperty(runtime, "size");

        if (size.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBufferDescriptor::size is required");
        }
      }
      if (value.hasProperty(runtime, "usage")) {
        auto usage = value.getProperty(runtime, "usage");

        if (usage.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBufferDescriptor::usage is required");
        }
      }
      if (value.hasProperty(runtime, "mappedAtCreation")) {
        auto mappedAtCreation = value.getProperty(runtime, "mappedAtCreation");
        if (value.hasProperty(runtime, "mappedAtCreation")) {
          result->_instance.mappedAtCreation = mappedAtCreation.getBool();
        }
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
