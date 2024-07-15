#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUQuerySet.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPURenderPassTimestampWrites {
public:
  wgpu::RenderPassTimestampWrites *getInstance() { return &_instance; }

  wgpu::RenderPassTimestampWrites _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites>> {
  static std::shared_ptr<rnwgpu::GPURenderPassTimestampWrites>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassTimestampWrites>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "querySet")) {
        auto querySet = value.getProperty(runtime, "querySet");

        if (querySet.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUQuerySet>>::fromJSI(
                  runtime, querySet, false);
          result->_instance.querySet = val->get();
        }

        if (querySet.isUndefined()) {
          throw std::runtime_error(
              "Property GPURenderPassTimestampWrites::querySet is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPURenderPassTimestampWrites::querySet is not defined");
      }
      if (value.hasProperty(runtime, "beginningOfPassWriteIndex")) {
        auto beginningOfPassWriteIndex =
            value.getProperty(runtime, "beginningOfPassWriteIndex");

        if (beginningOfPassWriteIndex.isNumber()) {
          result->_instance.beginningOfPassWriteIndex =
              static_cast<uint32_t>(beginningOfPassWriteIndex.getNumber());
        }
      }
      if (value.hasProperty(runtime, "endOfPassWriteIndex")) {
        auto endOfPassWriteIndex =
            value.getProperty(runtime, "endOfPassWriteIndex");

        if (endOfPassWriteIndex.isNumber()) {
          result->_instance.endOfPassWriteIndex =
              static_cast<uint32_t>(endOfPassWriteIndex.getNumber());
        }
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
