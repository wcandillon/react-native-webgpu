#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

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
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUExtent3DDictStrict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "depth")) {
        auto depth = value.getProperty(runtime, "depth");

        if (depth.isUndefined()) {
          throw std::runtime_error(
              "Property GPUExtent3DDictStrict::depth is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUExtent3DDictStrict::depth is not defined");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for GPUExtent3DDictStrict");
    //}
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUExtent3DDictStrict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
