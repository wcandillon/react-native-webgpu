#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
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

bool conv(wgpu::FragmentState &out, const GPUFragmentState &in) {
  out.targetCount = in.targets.size();

  return conv(out.targets, in.targets) && conv(out.module, in.module) &&
         conv(out.entryPoint, in.entryPoint) &&
         conv(out.constants, in.constants);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUFragmentState>> {
  static std::shared_ptr<rnwgpu::GPUFragmentState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUFragmentState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // targets std::vector<std::variant<std::nullptr_t,
      // std::shared_ptr<GPUColorTargetState>>>
      // module std::shared_ptr<GPUShaderModule>
      // entryPoint std::optional<std::string>
      // constants std::optional<std::map<std::string, double>>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUFragmentState> arg) {
    throw std::runtime_error("Invalid GPUFragmentState::toJSI()");
  }
};

} // namespace margelo