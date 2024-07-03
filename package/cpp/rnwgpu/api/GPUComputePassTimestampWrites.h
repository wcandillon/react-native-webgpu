#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

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
      }
      if (value.hasProperty(runtime, "beginningOfPassWriteIndex")) {
        auto beginningOfPassWriteIndex =
            value.getProperty(runtime, "beginningOfPassWriteIndex");

        if (value.hasProperty(runtime, "beginningOfPassWriteIndex")) {
          result->_instance.beginningOfPassWriteIndex =
              beginningOfPassWriteIndex.getNumber();
        }
      }
      if (value.hasProperty(runtime, "endOfPassWriteIndex")) {
        auto endOfPassWriteIndex =
            value.getProperty(runtime, "endOfPassWriteIndex");

        if (value.hasProperty(runtime, "endOfPassWriteIndex")) {
          result->_instance.endOfPassWriteIndex =
              endOfPassWriteIndex.getNumber();
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
