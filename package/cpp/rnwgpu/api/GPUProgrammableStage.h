#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUProgrammableStage {
public:
  wgpu::ProgrammableStage *getInstance() { return &_instance; }

private:
  wgpu::ProgrammableStage _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUProgrammableStage>> {
  static std::shared_ptr<rnwgpu::GPUProgrammableStage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUProgrammableStage>();
    if (value.hasProperty(runtime, "module")) {
      auto module = value.getProperty(runtime, "module");
      if (module.isNumber()) {
        result->_instance.module = module.getNumber();
      }
    }
    if (value.hasProperty(runtime, "entryPoint")) {
      auto entryPoint = value.getProperty(runtime, "entryPoint");
      if (entryPoint.isNumber()) {
        result->_instance.entryPoint = entryPoint.getNumber();
      } else if (entryPoint.isNull() || entryPoint.isUndefined()) {
        throw std::runtime_error(
            "Property GPUProgrammableStage::entryPoint is required");
      }
    }
    if (value.hasProperty(runtime, "constants")) {
      auto constants = value.getProperty(runtime, "constants");
      if (constants.isNumber()) {
        result->_instance.constants = constants.getNumber();
      } else if (constants.isNull() || constants.isUndefined()) {
        throw std::runtime_error(
            "Property GPUProgrammableStage::constants is required");
      }
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
