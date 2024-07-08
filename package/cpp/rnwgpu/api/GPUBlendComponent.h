#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUBlendComponent {
public:
  wgpu::BlendComponent *getInstance() { return &_instance; }

  wgpu::BlendComponent _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBlendComponent>> {
  static std::shared_ptr<rnwgpu::GPUBlendComponent>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBlendComponent>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "operation")) {
        auto operation = value.getProperty(runtime, "operation");
      }
      if (value.hasProperty(runtime, "srcFactor")) {
        auto srcFactor = value.getProperty(runtime, "srcFactor");
      }
      if (value.hasProperty(runtime, "dstFactor")) {
        auto dstFactor = value.getProperty(runtime, "dstFactor");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for GPUBlendComponent");
    //}
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendComponent> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
