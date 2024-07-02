#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUExtent3DDictStrict {
public:
  wgpu::Extent3DDictStrict *getInstance() { return &_instance; }

  wgpu::Extent3DDictStrict _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExtent3DDictStrict>> {
  static std::shared_ptr<rnwgpu::GPUExtent3DDictStrict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto result = std::make_unique<rnwgpu::GPUExtent3DDictStrict>();
    if (&arg != nullptr && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "depth")) {
        auto depth = value.getProperty(runtime, "depth");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUExtent3DDictStrict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
