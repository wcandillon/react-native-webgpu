#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUShaderModule.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUProgrammableStage {
  std::shared_ptr<GPUShaderModule> module; // GPUShaderModule
  std::optional<std::string> entryPoint;   // string
  std::optional<std::map<std::string, double>>
      constants; // Record< string, GPUPipelineConstantValue >
};

static bool conv(wgpu::ProgrammableStageDescriptor &out,
                 const std::shared_ptr<GPUProgrammableStage> &in) {
  // TODO: implement
  return false;
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUProgrammableStage>> {
  static std::shared_ptr<rnwgpu::GPUProgrammableStage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUProgrammableStage>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // module std::shared_ptr<GPUShaderModule>
      // entryPoint std::optional<std::string>
      // constants std::optional<std::map<std::string, double>>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUProgrammableStage> arg) {
    throw std::runtime_error("Invalid GPUProgrammableStage::toJSI()");
  }
};

} // namespace margelo