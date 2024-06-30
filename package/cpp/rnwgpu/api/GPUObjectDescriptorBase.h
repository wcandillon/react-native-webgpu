#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUObjectDescriptorBase {
public:
  wgpu::ObjectDescriptorBase *getInstance() { return &_instance; }

  wgpu::ObjectDescriptorBase _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUObjectDescriptorBase>> {
  static std::shared_ptr<rnwgpu::GPUObjectDescriptorBase>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUObjectDescriptorBase>();
    if (value.hasProperty(runtime, "label")) {
      auto label = value.getProperty(runtime, "label");
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUObjectDescriptorBase> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
