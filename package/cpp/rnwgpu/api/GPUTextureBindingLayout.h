#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

// TODO: Delete this class and use std::shared_ptr<wgpu::TextureBindingLayout>
// instead
class GPUTextureBindingLayout {
public:
  wgpu::TextureBindingLayout *getInstance() { return &_instance; }

  wgpu::TextureBindingLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "sampleType")) {
        auto sampleType = value.getProperty(runtime, "sampleType");
      }
      if (value.hasProperty(runtime, "viewDimension")) {
        auto viewDimension = value.getProperty(runtime, "viewDimension");
      }
      if (value.hasProperty(runtime, "multisampled")) {
        auto multisampled = value.getProperty(runtime, "multisampled");
      }
    }
    rnwgpu::Logger::logToConsole("GPUTextureBindingLayout::sampleType = %f",
                                 result->_instance.sampleType);
    rnwgpu::Logger::logToConsole("GPUTextureBindingLayout::viewDimension = %f",
                                 result->_instance.viewDimension);
    rnwgpu::Logger::logToConsole("GPUTextureBindingLayout::multisampled = %f",
                                 result->_instance.multisampled);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUTextureBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
