#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBufferBindingLayout {
public:
  wgpu::BufferBindingLayout *getInstance() { return &_instance; }

private:
  wgpu::BufferBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUBufferBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUBufferBindingLayout>();
    if (value.hasProperty(runtime, "type")) {
      auto type = value.getProperty(runtime, "type");
      if (type.isNumber()) {
        result->_instance.type = type.getNumber();
      } else if (type.isNull() || type.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBufferBindingLayout::type is required");
      }
    }
    if (value.hasProperty(runtime, "hasDynamicOffset")) {
      auto hasDynamicOffset = value.getProperty(runtime, "hasDynamicOffset");
      if (hasDynamicOffset.isNumber()) {
        result->_instance.hasDynamicOffset = hasDynamicOffset.getNumber();
      } else if (hasDynamicOffset.isNull() || hasDynamicOffset.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBufferBindingLayout::hasDynamicOffset is required");
      }
    }
    if (value.hasProperty(runtime, "minBindingSize")) {
      auto minBindingSize = value.getProperty(runtime, "minBindingSize");
      if (minBindingSize.isNumber()) {
        result->_instance.minBindingSize = minBindingSize.getNumber();
      } else if (minBindingSize.isNull() || minBindingSize.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBufferBindingLayout::minBindingSize is required");
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
