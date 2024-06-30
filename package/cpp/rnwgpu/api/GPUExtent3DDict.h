#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUExtent3DDict {
public:
  wgpu::Extent3DDict *getInstance() { return &_instance; }

private:
  wgpu::Extent3DDict _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUExtent3DDict>> {
  static std::shared_ptr<rnwgpu::GPUExtent3DDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUExtent3DDict>();
    if (value.hasProperty(runtime, "width")) {
      auto width = value.getProperty(runtime, "width");
      if (width.isNumber()) {
        result->_instance.width = width.getNumber();
      }
    }
    if (value.hasProperty(runtime, "height")) {
      auto height = value.getProperty(runtime, "height");
      if (height.isNumber()) {
        result->_instance.height = height.getNumber();
      } else if (height.isNull() || height.isUndefined()) {
        throw std::runtime_error(
            "Property GPUExtent3DDict::height is required");
      }
    }
    if (value.hasProperty(runtime, "depthOrArrayLayers")) {
      auto depthOrArrayLayers =
          value.getProperty(runtime, "depthOrArrayLayers");
      if (depthOrArrayLayers.isNumber()) {
        result->_instance.depthOrArrayLayers = depthOrArrayLayers.getNumber();
      } else if (depthOrArrayLayers.isNull() ||
                 depthOrArrayLayers.isUndefined()) {
        throw std::runtime_error(
            "Property GPUExtent3DDict::depthOrArrayLayers is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUExtent3DDict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
