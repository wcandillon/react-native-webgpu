#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUQuerySetDescriptor {
public:
  wgpu::QuerySetDescriptor *getInstance() { return &_instance; }

private:
  wgpu::QuerySetDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUQuerySetDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUQuerySetDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUQuerySetDescriptor>();
    if (value.hasProperty(runtime, "type")) {
      auto type = value.getProperty(runtime, "type");
      if (type.isNumber()) {
        result->_instance.type = type.getNumber();
      }
    }
    if (value.hasProperty(runtime, "count")) {
      auto count = value.getProperty(runtime, "count");
      if (count.isNumber()) {
        result->_instance.count = count.getNumber();
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUQuerySetDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
