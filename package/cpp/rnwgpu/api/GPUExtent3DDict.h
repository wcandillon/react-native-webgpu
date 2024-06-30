#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUExtent3DDict {
public:
  wgpu::Extent3DDict *getInstance() { return &_instance; }

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

      else if (width.isUndefined()) {
        throw std::runtime_error("Property GPUExtent3DDict::width is required");
      }
    }
    if (value.hasProperty(runtime, "height")) {
      auto height = value.getProperty(runtime, "height");
    }
    if (value.hasProperty(runtime, "depthOrArrayLayers")) {
      auto depthOrArrayLayers =
          value.getProperty(runtime, "depthOrArrayLayers");
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
