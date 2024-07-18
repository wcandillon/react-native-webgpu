#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "DescriptorConvertors.h"
#include "GPUQuerySet.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURenderPassTimestampWrites {
  std::shared_ptr<GPUQuerySet> querySet;           // GPUQuerySet
  std::optional<double> beginningOfPassWriteIndex; // GPUSize32
  std::optional<double> endOfPassWriteIndex;       // GPUSize32
};

static bool conv(wgpu::RenderPassTimestampWrites &out,
                 const std::shared_ptr<GPURenderPassTimestampWrites> &in) {
  return conv(out.querySet, in->querySet) &&
         conv(out.beginningOfPassWriteIndex, in->beginningOfPassWriteIndex) &&
         conv(out.endOfPassWriteIndex, in->endOfPassWriteIndex);
}
} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites>> {
  static std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassTimestampWrites>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "querySet")) {
        auto prop = value.getProperty(runtime, "querySet");
        result->querySet = JSIConverter<std::shared_ptr<GPUQuerySet>>::fromJSI(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "beginningOfPassWriteIndex")) {
        auto prop = value.getProperty(runtime, "beginningOfPassWriteIndex");
        result->beginningOfPassWriteIndex =
            JSIConverter<std::optional<double>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "endOfPassWriteIndex")) {
        auto prop = value.getProperty(runtime, "endOfPassWriteIndex");
        result->endOfPassWriteIndex =
            JSIConverter<std::optional<double>>::fromJSI(runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites> arg) {
    throw std::runtime_error("Invalid GPURenderPassTimestampWrites::toJSI()");
  }
};

} // namespace margelo