#pragma once

#include <memory>

#include "RNFJSIConverter.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

class GPUExtent3D {
public:
  wgpu::Extent3D *getInstance() { return &_instance; }

  wgpu::Extent3D _instance;
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUExtent3D>> {
  static std::shared_ptr<rnwgpu::GPUExtent3D>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUExtent3D>();
    if (!outOfBounds && arg.isObject()) {
      if (arg.getObject(runtime).isArray(runtime)) {
        auto array = arg.getObject(runtime).asArray(runtime);
        auto size = array.size(runtime);
        if (size == 0) {
          throw std::runtime_error(
              "Expected an array of size >1 for GPUTExtent3D");
        }
        if (size > 0) {
          result->_instance.width =
              array.getValueAtIndex(runtime, 0).asNumber();
        }
        if (size > 1) {
          result->_instance.height =
              array.getValueAtIndex(runtime, 1).asNumber();
        }
        if (size > 2) {
          result->_instance.depthOrArrayLayers =
              array.getValueAtIndex(runtime, 2).asNumber();
        }
      } else {
        auto object = arg.getObject(runtime);
        if (object.hasProperty(runtime, "width")) {
          result->_instance.width =
              object.getProperty(runtime, "width").asNumber();
        } else {
          throw std::runtime_error("Property GPUTExtent3D::width is required");
        }
        if (object.hasProperty(runtime, "height")) {
          result->_instance.height =
              object.getProperty(runtime, "height").asNumber();
        }
        if (object.hasProperty(runtime, "depthOrArrayLayers")) {
          result->_instance.depthOrArrayLayers =
              object.getProperty(runtime, "depthOrArrayLayers").asNumber();
        }
      }
    } else {
      throw std::runtime_error("Expected an object for GPUTExtent3D");
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUExtent3D> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo