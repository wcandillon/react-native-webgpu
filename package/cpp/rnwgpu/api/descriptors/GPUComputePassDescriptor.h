#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUComputePassTimestampWrites.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUComputePassDescriptor {
  std::optional<std::shared_ptr<GPUComputePassTimestampWrites>>
      timestampWrites;              // GPUComputePassTimestampWrites
  std::optional<std::string> label; // string
};

bool conv(wgpu::ComputePassDescriptor &out,
          const GPUComputePassDescriptor &in) {

  return conv(out.timestampWrites, in.timestampWrites) &&
         conv(out.label, in.label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUComputePassDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUComputePassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUComputePassDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // timestampWrites
      // std::optional<std::shared_ptr<GPUComputePassTimestampWrites>>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUComputePassDescriptor> arg) {
    throw std::runtime_error("Invalid GPUComputePassDescriptor::toJSI()");
  }
};

} // namespace margelo