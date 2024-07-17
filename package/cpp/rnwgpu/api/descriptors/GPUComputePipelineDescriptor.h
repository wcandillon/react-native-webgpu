#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUPipelineLayout.h"
#include "GPUProgrammableStage.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUComputePipelineDescriptor {
  std::shared_ptr<GPUProgrammableStage> compute; // GPUProgrammableStage
  std::variant<std::null_ptr, std::shared_ptr<GPUPipelineLayout>>
      layout;                       // | GPUPipelineLayout | GPUAutoLayoutMode
  std::optional<std::string> label; // string
};

} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUComputePipelineDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUComputePipelineDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUComputePipelineDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "compute")) {
        auto prop = value.getProperty(runtime, "compute");
        result->compute =
            JSIConverter::fromJSI<std::shared_ptr<GPUProgrammableStage>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "layout")) {
        auto prop = value.getProperty(runtime, "layout");
        result->layout = JSIConverter::fromJSI<
            std::variant<std::null_ptr, std::shared_ptr<GPUPipelineLayout>>>(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter::fromJSI<std::optional<std::string>>(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUComputePipelineDescriptor> arg) {
    throw std::runtime_error("Invalid GPUComputePipelineDescriptor::toJSI()");
  }
};
} // namespace margelo