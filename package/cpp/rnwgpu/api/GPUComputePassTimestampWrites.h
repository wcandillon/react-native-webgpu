#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<wgpu::ComputePassTimestampWrites>> {
  static std::shared_ptr<wgpu::ComputePassTimestampWrites>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::ComputePassTimestampWrites>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "querySet")) {
        auto querySet = value.getProperty(runtime, "querySet");

        if (querySet.isUndefined()) {
          throw std::runtime_error(
              "Property GPUComputePassTimestampWrites::querySet is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUComputePassTimestampWrites::querySet is not defined");
      }
      if (value.hasProperty(runtime, "beginningOfPassWriteIndex")) {
        auto beginningOfPassWriteIndex =
            value.getProperty(runtime, "beginningOfPassWriteIndex");

        if (beginningOfPassWriteIndex.isNumber()) {
          result->beginningOfPassWriteIndex =
              static_cast<wgpu::Size32>(beginningOfPassWriteIndex.getNumber());
        }
      }
      if (value.hasProperty(runtime, "endOfPassWriteIndex")) {
        auto endOfPassWriteIndex =
            value.getProperty(runtime, "endOfPassWriteIndex");

        if (endOfPassWriteIndex.isNumber()) {
          result->endOfPassWriteIndex =
              static_cast<wgpu::Size32>(endOfPassWriteIndex.getNumber());
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUComputePassTimestampWrites::querySet = %f",
                                 result->querySet);
    rnwgpu::Logger::logToConsole(
        "GPUComputePassTimestampWrites::beginningOfPassWriteIndex = %f",
        result->beginningOfPassWriteIndex);
    rnwgpu::Logger::logToConsole(
        "GPUComputePassTimestampWrites::endOfPassWriteIndex = %f",
        result->endOfPassWriteIndex);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<wgpu::ComputePassTimestampWrites> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
