#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBufferBindingLayout {
public:
  wgpu::BufferBindingLayout *getInstance() { return &_instance; }

  wgpu::BufferBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUBufferBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUBufferBindingLayout>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "type")) {
        auto type = value.getProperty(runtime, "type");
      }
      if (value.hasProperty(runtime, "hasDynamicOffset")) {
        auto hasDynamicOffset = value.getProperty(runtime, "hasDynamicOffset");
        if (value.hasProperty(runtime, "hasDynamicOffset")) {
          result->_instance.hasDynamicOffset = hasDynamicOffset.getBool();
        }
      }
      if (value.hasProperty(runtime, "minBindingSize")) {
        auto minBindingSize = value.getProperty(runtime, "minBindingSize");

        if (value.hasProperty(runtime, "minBindingSize")) {
          result->_instance.minBindingSize = minBindingSize.getNumber();
        }
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
