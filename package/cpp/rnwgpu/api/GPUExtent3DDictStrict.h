#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUExtent3DDictStrict {
public:
  wgpu::Extent3DDictStrict *getInstance() { return &_instance; }

private:
  wgpu::Extent3DDictStrict _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExtent3DDictStrict>> {
  static std::shared_ptr<rnwgpu::GPUExtent3DDictStrict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUExtent3DDictStrict>();
    if (value.hasProperty(runtime, "depth")) {
      auto depth = value.getProperty(runtime, "depth");
      if (depth.isNumber()) {
        result->_instance.depth = depth.getNumber();
      } else if (depth.isNull() || depth.isUndefined()) {
        throw std::runtime_error(
            "Property GPUExtent3DDictStrict::depth is required");
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
