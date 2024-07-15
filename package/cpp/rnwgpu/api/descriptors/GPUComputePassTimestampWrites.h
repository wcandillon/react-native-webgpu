#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUComputePassTimestampWrites {
public:
  wgpu::ComputePassTimestampWrites *getInstance() { return &_instance; }

  wgpu::ComputePassTimestampWrites _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUComputePassTimestampWrites>> {
  static std::shared_ptr<rnwgpu::GPUComputePassTimestampWrites>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUComputePassTimestampWrites>();
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
          result->_instance.beginningOfPassWriteIndex =
              static_cast<wgpu::Size32>(beginningOfPassWriteIndex.getNumber());
        }
      }
      if (value.hasProperty(runtime, "endOfPassWriteIndex")) {
        auto endOfPassWriteIndex =
            value.getProperty(runtime, "endOfPassWriteIndex");

        if (endOfPassWriteIndex.isNumber()) {
          result->_instance.endOfPassWriteIndex =
              static_cast<wgpu::Size32>(endOfPassWriteIndex.getNumber());
        }
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUComputePassTimestampWrites> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
