#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUOrigin3DDict {
public:
  wgpu::Origin3DDict *getInstance() { return &_instance; }

  wgpu::Origin3DDict _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUOrigin3DDict>> {
  static std::shared_ptr<rnwgpu::GPUOrigin3DDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUOrigin3DDict>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "x")) {
        auto x = value.getProperty(runtime, "x");

        if (value.hasProperty(runtime, "x")) {
          result->_instance.x = x.getNumber();
        }
      }
      if (value.hasProperty(runtime, "y")) {
        auto y = value.getProperty(runtime, "y");

        if (value.hasProperty(runtime, "y")) {
          result->_instance.y = y.getNumber();
        }
      }
      if (value.hasProperty(runtime, "z")) {
        auto z = value.getProperty(runtime, "z");

        if (value.hasProperty(runtime, "z")) {
          result->_instance.z = z.getNumber();
        }
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUOrigin3DDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
