#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassTimestampWrites {
public:
  wgpu::RenderPassTimestampWrites *getInstance() { return &_instance; }

private:
  wgpu::RenderPassTimestampWrites _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites>> {
  static std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURenderPassTimestampWrites>();
    if (value.hasProperty(runtime, "querySet")) {
      auto querySet = value.getProperty(runtime, "querySet");
      if (querySet.isNumber()) {
        result->_instance.querySet = querySet.getNumber();
      }
    }
    if (value.hasProperty(runtime, "beginningOfPassWriteIndex")) {
      auto beginningOfPassWriteIndex =
          value.getProperty(runtime, "beginningOfPassWriteIndex");
      if (beginningOfPassWriteIndex.isNumber()) {
        result->_instance.beginningOfPassWriteIndex =
            beginningOfPassWriteIndex.getNumber();
      } else if (beginningOfPassWriteIndex.isNull() ||
                 beginningOfPassWriteIndex.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassTimestampWrites::beginningOfPassWriteIndex "
            "is required");
      }
    }
    if (value.hasProperty(runtime, "endOfPassWriteIndex")) {
      auto endOfPassWriteIndex =
          value.getProperty(runtime, "endOfPassWriteIndex");
      if (endOfPassWriteIndex.isNumber()) {
        result->_instance.endOfPassWriteIndex = endOfPassWriteIndex.getNumber();
      } else if (endOfPassWriteIndex.isNull() ||
                 endOfPassWriteIndex.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassTimestampWrites::endOfPassWriteIndex is "
            "required");
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
