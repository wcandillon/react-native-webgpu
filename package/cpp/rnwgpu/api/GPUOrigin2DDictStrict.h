#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUOrigin2DDictStrict {
public:
  wgpu::Origin2DDictStrict *getInstance() { return &_instance; }

  wgpu::Origin2DDictStrict _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict>> {
  static std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUOrigin2DDictStrict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "z")) {
        auto z = value.getProperty(runtime, "z");

        if (z.isUndefined()) {
          throw std::runtime_error(
              "Property GPUOrigin2DDictStrict::z is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUOrigin2DDictStrict::z is not defined");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for GPUOrigin2DDictStrict");
    //}
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
