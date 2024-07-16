#pragma once

#include <memory>

#include "RNFJSIConverter.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

class GPUOrigin3D {
public:
  wgpu::Origin3D *getInstance() { return &_instance; }

  wgpu::Origin3D _instance;
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUOrigin3D>> {
  static std::shared_ptr<rnwgpu::GPUOrigin3D>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUOrigin3D>();
    if (!outOfBounds && arg.isObject()) {
      if (arg.getObject(runtime).isArray(runtime)) {
        auto array = arg.getObject(runtime).asArray(runtime);
        auto size = array.size(runtime);
        if (size == 0) {
          throw std::runtime_error(
              "Expected an array of size >1 for GPUTExtent3D");
        }
        if (size > 0) {
          result->_instance.x = array.getValueAtIndex(runtime, 0).asNumber();
        }
        if (size > 1) {
          result->_instance.y = array.getValueAtIndex(runtime, 1).asNumber();
        }
        if (size > 2) {
          result->_instance.z = array.getValueAtIndex(runtime, 2).asNumber();
        }
      } else {
        auto object = arg.getObject(runtime);
        if (object.hasProperty(runtime, "x")) {
          result->_instance.x = object.getProperty(runtime, "x").asNumber();
        }
        if (object.hasProperty(runtime, "y")) {
          result->_instance.y = object.getProperty(runtime, "y").asNumber();
        }
        if (object.hasProperty(runtime, "z")) {
          result->_instance.z = object.getProperty(runtime, "z").asNumber();
        }
      }
    } else {
      throw std::runtime_error("Expected an object for GPUOrigin3D");
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUOrigin3D> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo