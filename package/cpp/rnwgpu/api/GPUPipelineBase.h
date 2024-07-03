#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUPipelineBase {
public:
  wgpu::PipelineBase *getInstance() { return &_instance; }

  wgpu::PipelineBase _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUPipelineBase>> {
  static std::shared_ptr<rnwgpu::GPUPipelineBase>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUPipelineBase>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUPipelineBase> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
