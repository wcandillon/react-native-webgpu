#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUUncapturedErrorEventInit {
public:
  wgpu::UncapturedErrorEventInit *getInstance() { return &_instance; }

  wgpu::UncapturedErrorEventInit _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>> {
  static std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUUncapturedErrorEventInit>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "error")) {
        auto error = value.getProperty(runtime, "error");

        else if (error.isUndefined()) {
          throw std::runtime_error(
              "Property GPUUncapturedErrorEventInit::error is required");
        }
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
