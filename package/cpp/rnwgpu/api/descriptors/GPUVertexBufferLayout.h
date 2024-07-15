#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUVertexBufferLayout {
public:
  wgpu::VertexBufferLayout *getInstance() { return &_instance; }

  wgpu::VertexBufferLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexBufferLayout>> {
  static std::shared_ptr<rnwgpu::GPUVertexBufferLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUVertexBufferLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "arrayStride")) {
        auto arrayStride = value.getProperty(runtime, "arrayStride");

        if (arrayStride.isNumber()) {
          result->_instance.arrayStride = arrayStride.getNumber();
        }

        if (arrayStride.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexBufferLayout::arrayStride is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUVertexBufferLayout::arrayStride is not defined");
      }
      if (value.hasProperty(runtime, "stepMode")) {
        auto stepMode = value.getProperty(runtime, "stepMode");

        if (stepMode.isString()) {
          auto str = stepMode.asString(runtime).utf8(runtime);
          wgpu::VertexStepMode enumValue;
          convertJSUnionToEnum(str, &enumValue);
          result->_instance.stepMode = enumValue;
        }
      }
      if (value.hasProperty(runtime, "attributes")) {
        auto attributes = value.getProperty(runtime, "attributes");

        if (attributes.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexBufferLayout::attributes is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUVertexBufferLayout::attributes is not defined");
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexBufferLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
