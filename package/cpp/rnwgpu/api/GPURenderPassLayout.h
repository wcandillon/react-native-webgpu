#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPURenderPassLayout {
public:
  wgpu::RenderPassLayout *getInstance() { return &_instance; }

  wgpu::RenderPassLayout _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassLayout>> {
  static std::shared_ptr<rnwgpu::GPURenderPassLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPURenderPassLayout>();
    if (value.hasProperty(runtime, "colorFormats")) {
      auto colorFormats = value.getProperty(runtime, "colorFormats");

      else if (colorFormats.isUndefined()) {
        throw std::runtime_error(
            "Property GPURenderPassLayout::colorFormats is required");
      }
    }
    if (value.hasProperty(runtime, "depthStencilFormat")) {
      auto depthStencilFormat =
          value.getProperty(runtime, "depthStencilFormat");
    }
    if (value.hasProperty(runtime, "sampleCount")) {
      auto sampleCount = value.getProperty(runtime, "sampleCount");
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPURenderPassLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
