#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUColorTargetState.h"
#include "GPUShaderModule.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUFragmentState {
  std::vector<
      std::variant<std::nullptr_t, std::shared_ptr<GPUColorTargetState>>>
      targets; // Iterable<GPUColorTargetState | null>
  std::shared_ptr<GPUShaderModule> module; // GPUShaderModule
  std::optional<std::string> entryPoint;   // string
  std::optional<std::map<std::string, double>>
      constants; // Record< string, GPUPipelineConstantValue >
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUFragmentState>> {
  static std::shared_ptr<rnwgpu::GPUFragmentState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUFragmentState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "targets")) {
        auto prop = value.getProperty(runtime, "targets");
        result->targets = JSIConverter::fromJSI<std::vector<std::variant<
            std::nullptr_t, std::shared_ptr<GPUColorTargetState>>>>(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "module")) {
        auto prop = value.getProperty(runtime, "module");
        result->module =
            JSIConverter::fromJSI<std::shared_ptr<GPUShaderModule>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "entryPoint")) {
        auto prop = value.getProperty(runtime, "entryPoint");
        result->entryPoint = JSIConverter::fromJSI<std::optional<std::string>>(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "constants")) {
        auto prop = value.getProperty(runtime, "constants");
        result->constants =
            JSIConverter::fromJSI<std::optional<std::map<std::string, double>>>(
                runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUFragmentState> arg) {
    throw std::runtime_error("Invalid GPUFragmentState::toJSI()");
  }
};
} // namespace margelo