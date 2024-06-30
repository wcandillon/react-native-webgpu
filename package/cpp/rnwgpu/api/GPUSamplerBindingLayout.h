#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUSamplerBindingLayout {
public:
  wgpu::SamplerBindingLayout *getInstance() { return &_instance; }

private:
  wgpu::SamplerBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUSamplerBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUSamplerBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUSamplerBindingLayout>();
    if (value.hasProperty(runtime, "type")) {
      auto type = value.getProperty(runtime, "type");
      if (type.isNumber()) {
        result->_instance.type = type.getNumber();
      } else if (type.isNull() || type.isUndefined()) {
        throw std::runtime_error(
            "Property GPUSamplerBindingLayout::type is required");
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUSamplerBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
