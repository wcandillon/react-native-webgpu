#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBlendComponent {
public:
  wgpu::BlendComponent *getInstance() { return &_instance; }

private:
  wgpu::BlendComponent _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBlendComponent>> {
  static std::shared_ptr<rnwgpu::GPUBlendComponent>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUBlendComponent>();
    if (value.hasProperty(runtime, "operation")) {
      auto operation = value.getProperty(runtime, "operation");
      if (operation.isNumber()) {
        result->_instance.operation = operation.getNumber();
      } else if (operation.isNull() || operation.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBlendComponent::operation is required");
      }
    }
    if (value.hasProperty(runtime, "srcFactor")) {
      auto srcFactor = value.getProperty(runtime, "srcFactor");
      if (srcFactor.isNumber()) {
        result->_instance.srcFactor = srcFactor.getNumber();
      } else if (srcFactor.isNull() || srcFactor.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBlendComponent::srcFactor is required");
      }
    }
    if (value.hasProperty(runtime, "dstFactor")) {
      auto dstFactor = value.getProperty(runtime, "dstFactor");
      if (dstFactor.isNumber()) {
        result->_instance.dstFactor = dstFactor.getNumber();
      } else if (dstFactor.isNull() || dstFactor.isUndefined()) {
        throw std::runtime_error(
            "Property GPUBlendComponent::dstFactor is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendComponent> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
