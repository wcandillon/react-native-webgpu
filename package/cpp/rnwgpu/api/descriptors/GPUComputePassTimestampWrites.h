#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUQuerySet.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUComputePassTimestampWrites {
  std::shared_ptr<GPUQuerySet> querySet;           // GPUQuerySet
  std::optional<double> beginningOfPassWriteIndex; // GPUSize32
  std::optional<double> endOfPassWriteIndex;       // GPUSize32
};

bool conv(wgpu::ComputePassTimestampWrites &out,
          GPUComputePassTimestampWrites &in) {

  return conv(out.querySet, in.querySet) &&
         conv(out.beginningOfPassWriteIndex, in.beginningOfPassWriteIndex) &&
         conv(out.endOfPassWriteIndex, in.endOfPassWriteIndex);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUComputePassTimestampWrites>> {
  static std::shared_ptr<rnwgpu::GPUComputePassTimestampWrites>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUComputePassTimestampWrites>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // querySet std::shared_ptr<GPUQuerySet>
      // beginningOfPassWriteIndex std::optional<double>
      // endOfPassWriteIndex std::optional<double>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUComputePassTimestampWrites> arg) {
    throw std::runtime_error("Invalid GPUComputePassTimestampWrites::toJSI()");
  }
};

} // namespace margelo