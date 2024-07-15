#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

class GPUShaderModuleDescriptor {
public:
  wgpu::ShaderModuleDescriptor *getInstance() { return &_instance; }

  wgpu::ShaderModuleDescriptor _instance;

  std::string label;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUShaderModuleDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "code")) {
        auto code = value.getProperty(runtime, "code");

        if (code.isString()) {
          auto str = code.asString(runtime).utf8(runtime);
          result->code = str;
          result->_instance.code = result->code.c_str();
        }

        if (code.isUndefined()) {
          throw std::runtime_error(
              "Property GPUShaderModuleDescriptor::code is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUShaderModuleDescriptor::code is not defined");
      }
      if (value.hasProperty(runtime, "sourceMap")) {
        auto sourceMap = value.getProperty(runtime, "sourceMap");

        if (sourceMap.isUndefined()) {
          throw std::runtime_error(
              "Property GPUShaderModuleDescriptor::sourceMap is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUShaderModuleDescriptor::sourceMap is not defined");
      }
      if (value.hasProperty(runtime, "compilationHints")) {
        auto compilationHints = value.getProperty(runtime, "compilationHints");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str;
          result->_instance.label = result->label.c_str();
        }
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUShaderModuleDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
