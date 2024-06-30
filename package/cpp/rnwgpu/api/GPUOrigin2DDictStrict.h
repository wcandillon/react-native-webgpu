#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUOrigin2DDictStrict {
public:
  wgpu::Origin2DDictStrict *getInstance() { return &_instance; }

private:
  wgpu::Origin2DDictStrict _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict>> {
  static std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUOrigin2DDictStrict>();
    if (value.hasProperty(runtime, "z")) {
      auto z = value.getProperty(runtime, "z");
      if (z.isNumber()) {
        result->_instance.z = z.getNumber();
      } else if (z.isNull() || z.isUndefined()) {
        throw std::runtime_error(
            "Property GPUOrigin2DDictStrict::z is required");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUOrigin2DDictStrict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
