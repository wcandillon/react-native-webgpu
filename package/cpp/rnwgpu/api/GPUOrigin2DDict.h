#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUOrigin2DDict {
public:
  wgpu::Origin2DDict *getInstance() { return &_instance; }

private:
  wgpu::Origin2DDict _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUOrigin2DDict>> {
  static std::shared_ptr<rnwgpu::GPUOrigin2DDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUOrigin2DDict>();
    if (value.hasProperty(runtime, "x")) {
      auto x = value.getProperty(runtime, "x");
      if (x.isNumber()) {
        result->_instance.x = x.getNumber();
      } else if (x.isNull() || x.isUndefined()) {
        throw std::runtime_error("Property GPUOrigin2DDict::x is required");
      }
    }
    if (value.hasProperty(runtime, "y")) {
      auto y = value.getProperty(runtime, "y");
      if (y.isNumber()) {
        result->_instance.y = y.getNumber();
      } else if (y.isNull() || y.isUndefined()) {
        throw std::runtime_error("Property GPUOrigin2DDict::y is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUOrigin2DDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
