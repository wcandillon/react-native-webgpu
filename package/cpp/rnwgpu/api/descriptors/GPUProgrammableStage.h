#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUProgrammableStage {
public:
  wgpu::ProgrammableStage *getInstance() { return &_instance; }

  wgpu::ProgrammableStage _instance;

  std::string entryPoint;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUProgrammableStage>> {
  static std::shared_ptr<rnwgpu::GPUProgrammableStage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUProgrammableStage>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "module")) {
        auto module = value.getProperty(runtime, "module");

        if (module.isUndefined()) {
          throw std::runtime_error(
              "Property GPUProgrammableStage::module is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUProgrammableStage::module is not defined");
      }
      if (value.hasProperty(runtime, "entryPoint")) {
        auto entryPoint = value.getProperty(runtime, "entryPoint");

        if (entryPoint.isString()) {
          auto str = entryPoint.asString(runtime).utf8(runtime);
          result->entryPoint = str;
          result->_instance.entryPoint = result->entryPoint.c_str();
        }
      }
      if (value.hasProperty(runtime, "constants")) {
        auto constants = value.getProperty(runtime, "constants");
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
