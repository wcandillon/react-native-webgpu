#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUColorDict {
public:
  wgpu::ColorDict *getInstance() { return &_instance; }

  wgpu::ColorDict _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUColorDict>> {
  static std::shared_ptr<rnwgpu::GPUColorDict> fromJSI(jsi::Runtime &runtime,
                                                       const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUColorDict>();
    if (value.hasProperty(runtime, "r")) {
      auto r = value.getProperty(runtime, "r");

      else if (r.isUndefined()) {
        throw std::runtime_error("Property GPUColorDict::r is required");
      }
    }
    if (value.hasProperty(runtime, "g")) {
      auto g = value.getProperty(runtime, "g");

      else if (g.isUndefined()) {
        throw std::runtime_error("Property GPUColorDict::g is required");
      }
    }
    if (value.hasProperty(runtime, "b")) {
      auto b = value.getProperty(runtime, "b");

      else if (b.isUndefined()) {
        throw std::runtime_error("Property GPUColorDict::b is required");
      }
    }
    if (value.hasProperty(runtime, "a")) {
      auto a = value.getProperty(runtime, "a");

      else if (a.isUndefined()) {
        throw std::runtime_error("Property GPUColorDict::a is required");
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
