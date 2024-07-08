#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUPipelineDescriptorBase {
public:
  wgpu::PipelineDescriptorBase *getInstance() { return &_instance; }

  wgpu::PipelineDescriptorBase _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUPipelineDescriptorBase>> {
  static std::shared_ptr<rnwgpu::GPUPipelineDescriptorBase>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUPipelineDescriptorBase>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "layout")) {
        auto layout = value.getProperty(runtime, "layout");

        if (layout.isUndefined()) {
          throw std::runtime_error(
              "Property GPUPipelineDescriptorBase::layout is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUPipelineDescriptorBase::layout is not defined");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for
    // GPUPipelineDescriptorBase");
    //}
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUPipelineDescriptorBase> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
