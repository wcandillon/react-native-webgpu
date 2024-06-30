#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBindGroupEntry {
public:
  wgpu::BindGroupEntry *getInstance() { return &_instance; }

private:
  wgpu::BindGroupEntry _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupEntry>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUBindGroupEntry>();
    if (value.hasProperty(runtime, "binding")) {
      auto binding = value.getProperty(runtime, "binding");
      if (binding.isNumber()) {
        result->_instance.binding = binding.getNumber();
      }
    }
    if (value.hasProperty(runtime, "resource")) {
      auto resource = value.getProperty(runtime, "resource");
      if (resource.isNumber()) {
        result->_instance.resource = resource.getNumber();
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBindGroupEntry> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
