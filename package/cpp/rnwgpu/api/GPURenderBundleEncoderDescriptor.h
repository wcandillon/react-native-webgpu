#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderBundleEncoderDescriptor {
public:
  wgpu::RenderBundleEncoderDescriptor *getInstance() { return &_instance; }

  wgpu::RenderBundleEncoderDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderBundleEncoderDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "depthReadOnly")) {
        auto depthReadOnly = value.getProperty(runtime, "depthReadOnly");
      }
      if (value.hasProperty(runtime, "stencilReadOnly")) {
        auto stencilReadOnly = value.getProperty(runtime, "stencilReadOnly");
      }
    }
    rnwgpu::Logger::logToConsole(
        "GPURenderBundleEncoderDescriptor::depthReadOnly = %f",
        result->_instance.depthReadOnly);
    rnwgpu::Logger::logToConsole(
        "GPURenderBundleEncoderDescriptor::stencilReadOnly = %f",
        result->_instance.stencilReadOnly);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderBundleEncoderDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
