#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "GPUPipelineLayout.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUShaderModuleCompilationHint {
  std::string entryPoint; // string
  std::optional<std::variant<std::null_ptr, std::shared_ptr<GPUPipelineLayout>>>
      layout; // | GPUPipelineLayout | GPUAutoLayoutMode
};

} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUShaderModuleCompilationHint>> {
  static std::shared_ptr<rnwgpu::GPUShaderModuleCompilationHint>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUShaderModuleCompilationHint>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "entryPoint")) {
        auto prop = value.getProperty(runtime, "entryPoint");
        result->entryPoint =
            JSIConverter::fromJSI<std::string>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "layout")) {
        auto prop = value.getProperty(runtime, "layout");
        result->layout = JSIConverter::fromJSI<std::optional<
            std::variant<std::null_ptr, std::shared_ptr<GPUPipelineLayout>>>>(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUShaderModuleCompilationHint> arg) {
    throw std::runtime_error("Invalid GPUShaderModuleCompilationHint::toJSI()");
  }
};
} // namespace margelo