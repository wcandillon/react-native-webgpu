#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBindGroupDescriptor {
public:
  wgpu::BindGroupDescriptor *getInstance() { return &_instance; }

private:
  wgpu::BindGroupDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUBindGroupDescriptor>();
    if (value.hasProperty(runtime, "layout")) {
      auto layout = value.getProperty(runtime, "layout");
      if (layout.isNumber()) {
        result->_instance.layout = layout.getNumber();
      }
    }
    if (value.hasProperty(runtime, "entries")) {
      auto entries = value.getProperty(runtime, "entries");
      if (entries.isNumber()) {
        result->_instance.entries = entries.getNumber();
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBindGroupDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
