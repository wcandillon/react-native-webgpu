#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUColorDict {
public:
  wgpu::ColorDict *getInstance() { return &_instance; }

private:
  wgpu::ColorDict _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUColorDict>> {
  static std::shared_ptr<rnwgpu::GPUColorDict> fromJSI(jsi::Runtime &runtime,
                                                       const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUColorDict>();
    if (value.hasProperty(runtime, "r")) {
      auto r = value.getProperty(runtime, "r");
      if (r.isNumber()) {
        result->_instance.r = r.getNumber();
      }
    }
    if (value.hasProperty(runtime, "g")) {
      auto g = value.getProperty(runtime, "g");
      if (g.isNumber()) {
        result->_instance.g = g.getNumber();
      }
    }
    if (value.hasProperty(runtime, "b")) {
      auto b = value.getProperty(runtime, "b");
      if (b.isNumber()) {
        result->_instance.b = b.getNumber();
      }
    }
    if (value.hasProperty(runtime, "a")) {
      auto a = value.getProperty(runtime, "a");
      if (a.isNumber()) {
        result->_instance.a = a.getNumber();
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUColorDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
