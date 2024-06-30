#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUPipelineErrorInit {
public:
  wgpu::PipelineErrorInit *getInstance() { return &_instance; }

private:
  wgpu::PipelineErrorInit _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUPipelineErrorInit>> {
  static std::shared_ptr<rnwgpu::GPUPipelineErrorInit>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUPipelineErrorInit>();
    if (value.hasProperty(runtime, "reason")) {
      auto reason = value.getProperty(runtime, "reason");
      if (reason.isNumber()) {
        result->_instance.reason = reason.getNumber();
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUPipelineErrorInit> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
