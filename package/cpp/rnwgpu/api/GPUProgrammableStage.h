#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUProgrammableStage {
public:
  wgpu::ProgrammableStage *getInstance() { return &_instance; }

  wgpu::ProgrammableStage _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUProgrammableStage>> {
  static std::shared_ptr<rnwgpu::GPUProgrammableStage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto value = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUProgrammableStage>();
    if (value.hasProperty(runtime, "module")) {
      auto module = value.getProperty(runtime, "module");

      else if (module.isUndefined()) {
        throw std::runtime_error(
            "Property GPUProgrammableStage::module is required");
      }
    }
    if (value.hasProperty(runtime, "entryPoint")) {
      auto entryPoint = value.getProperty(runtime, "entryPoint");

      if (value.hasProperty(runtime, "entryPoint")) {
        auto str = value.asString(runtime).utf8(runtime);
        result->_instance.entryPoint = str.c_str();
      }
    }
    if (value.hasProperty(runtime, "constants")) {
      auto constants = value.getProperty(runtime, "constants");
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUProgrammableStage> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
