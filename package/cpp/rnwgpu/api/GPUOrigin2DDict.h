#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUOrigin2DDict {
public:
  wgpu::Origin2DDict *getInstance() { return &_instance; }

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
    }
    if (value.hasProperty(runtime, "y")) {
      auto y = value.getProperty(runtime, "y");
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
