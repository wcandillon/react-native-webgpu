#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUPipelineErrorInit {
public:
  wgpu::PipelineErrorInit *getInstance() { return &_instance; }

  wgpu::PipelineErrorInit _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUPipelineErrorInit>> {
  static std::shared_ptr<rnwgpu::GPUPipelineErrorInit>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUPipelineErrorInit>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "reason")) {
        auto reason = value.getProperty(runtime, "reason");

        if (reason.isString()) {
          auto str = reason.asString(runtime).utf8(runtime);
          wgpu::PipelineErrorReason enumValue;
          convertJSUnionToEnum(str, &enumValue);
          result->_instance.reason = enumValue;
        }

        if (reason.isUndefined()) {
          throw std::runtime_error(
              "Property GPUPipelineErrorInit::reason is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUPipelineErrorInit::reason is not defined");
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
