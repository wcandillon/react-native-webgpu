#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUFragmentState {
public:
  wgpu::FragmentState *getInstance() { return &_instance; }

private:
  wgpu::FragmentState _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUFragmentState>> {
  static std::shared_ptr<rnwgpu::GPUFragmentState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUFragmentState>();
    if (value.hasProperty(runtime, "targets")) {
      auto targets = value.getProperty(runtime, "targets");
      if (targets.isNumber()) {
        result->_instance.targets = targets.getNumber();
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUFragmentState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
