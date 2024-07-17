#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUShaderModuleCompilationHint.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUShaderModuleDescriptor {
  std::string code; // string
  std::optional<std::vector<std::shared_ptr<GPUShaderModuleCompilationHint>>>
      compilationHints;             // Array<GPUShaderModuleCompilationHint>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUShaderModuleDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // code std::string
      // compilationHints
      // std::optional<std::vector<std::shared_ptr<GPUShaderModuleCompilationHint>>>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor> arg) {
    throw std::runtime_error("Invalid GPUShaderModuleDescriptor::toJSI()");
  }
};

} // namespace margelo