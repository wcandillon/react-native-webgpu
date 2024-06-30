#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUUncapturedErrorEventInit {
public:
  wgpu::UncapturedErrorEventInit *getInstance() { return &_instance; }

private:
  wgpu::UncapturedErrorEventInit _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>> {
  static std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUUncapturedErrorEventInit>();
    if (value.hasProperty(runtime, "error")) {
      auto error = value.getProperty(runtime, "error");
      if (error.isNumber()) {
        result->_instance.error = error.getNumber();
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
