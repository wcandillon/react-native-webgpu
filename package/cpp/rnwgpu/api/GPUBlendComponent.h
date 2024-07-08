#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

// TODO: Delete this class and use std::shared_ptr<wgpu::BlendComponent> instead
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
    rnwgpu::Logger::logToConsole("GPUBlendComponent::operation = %f",
                                 result->_instance.operation);
    rnwgpu::Logger::logToConsole("GPUBlendComponent::srcFactor = %f",
                                 result->_instance.srcFactor);
    rnwgpu::Logger::logToConsole("GPUBlendComponent::dstFactor = %f",
                                 result->_instance.dstFactor);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendComponent> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
