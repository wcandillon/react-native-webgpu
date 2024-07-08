#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUExtent3DDict {
public:
  wgpu::Extent3DDict *getInstance() { return &_instance; }

  wgpu::Extent3DDict _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUExtent3DDict>> {
  static std::shared_ptr<rnwgpu::GPUExtent3DDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUExtent3DDict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "width")) {
        auto width = value.getProperty(runtime, "width");

        if (width.isNumber()) {
          result->_instance.width =
              static_cast<wgpu::IntegerCoordinate>(width.getNumber());
        }

        if (width.isUndefined()) {
          throw std::runtime_error(
              "Property GPUExtent3DDict::width is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUExtent3DDict::width is not defined");
      }
      if (value.hasProperty(runtime, "height")) {
        auto height = value.getProperty(runtime, "height");
      }
      if (value.hasProperty(runtime, "depthOrArrayLayers")) {
        auto depthOrArrayLayers =
            value.getProperty(runtime, "depthOrArrayLayers");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for GPUExtent3DDict");
    //}
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUExtent3DDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
