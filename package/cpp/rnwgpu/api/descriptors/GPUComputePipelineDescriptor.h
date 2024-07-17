#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
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
  std::variant<std::nullptr_t, std::shared_ptr<GPUPipelineLayout>>
      layout;                       // | GPUPipelineLayout | GPUAutoLayoutMode
  std::optional<std::string> label; // string
};

bool conv(wgpu::ComputePipelineDescriptor &out,
          const GPUComputePipelineDescriptor &in) {

  return conv(out.compute, in.compute) && conv(out.layout, in.layout) &&
         conv(out.label, in.label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUComputePipelineDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUComputePipelineDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUComputePipelineDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // compute std::shared_ptr<GPUProgrammableStage>
      // layout std::variant<std::nullptr_t, std::shared_ptr<GPUPipelineLayout>>
      // label std::optional<std::string>
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