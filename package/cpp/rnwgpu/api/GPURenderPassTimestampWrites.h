#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassTimestampWrites {
public:
  wgpu::RenderPassTimestampWrites *getInstance() { return &_instance; }

  wgpu::RenderPassTimestampWrites _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites>> {
  static std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURenderPassTimestampWrites>();
    if (value.hasProperty(runtime, "querySet")) {
      auto querySet = value.getProperty(runtime, "querySet");

      else if (querySet.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassTimestampWrites::querySet is required");
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
        result->_instance.endOfPassWriteIndex = endOfPassWriteIndex.getNumber();
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
