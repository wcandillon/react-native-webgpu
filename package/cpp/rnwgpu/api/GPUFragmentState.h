#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::FragmentState>> {
  static std::shared_ptr<wgpu::FragmentState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::FragmentState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "targets")) {
        auto targets = value.getProperty(runtime, "targets");

        if (targets.isUndefined()) {
          throw std::runtime_error(
              "Property GPUFragmentState::targets is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUFragmentState::targets is not defined");
      }
      if (value.hasProperty(runtime, "module")) {
        auto module = value.getProperty(runtime, "module");

        if (module.isUndefined()) {
          throw std::runtime_error(
              "Property GPUFragmentState::module is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUFragmentState::module is not defined");
      }
      if (value.hasProperty(runtime, "entryPoint")) {
        auto entryPoint = value.getProperty(runtime, "entryPoint");

        if (entryPoint.isString()) {
          auto str = entryPoint.asString(runtime).utf8(runtime);
          result->entryPoint = str.c_str();
        }
      }
      if (value.hasProperty(runtime, "constants")) {
        auto constants = value.getProperty(runtime, "constants");
      }
    }
    rnwgpu::Logger::logToConsole("GPUFragmentState::targets = %f",
                                 result->targets);
    rnwgpu::Logger::logToConsole("GPUFragmentState::module = %f",
                                 result->module);
    rnwgpu::Logger::logToConsole("GPUFragmentState::entryPoint = %f",
                                 result->entryPoint);
    rnwgpu::Logger::logToConsole("GPUFragmentState::constants = %f",
                                 result->constants);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::FragmentState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
