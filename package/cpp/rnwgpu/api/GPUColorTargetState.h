#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::ColorTargetState>> {
  static std::shared_ptr<wgpu::ColorTargetState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::ColorTargetState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUColorTargetState::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUColorTargetState::format is not defined");
      }
      if (value.hasProperty(runtime, "blend")) {
        auto blend = value.getProperty(runtime, "blend");
      }
      if (value.hasProperty(runtime, "writeMask")) {
        auto writeMask = value.getProperty(runtime, "writeMask");

        if (writeMask.isNumber()) {
          result->writeMask =
              static_cast<wgpu::ColorWriteFlags>(writeMask.getNumber());
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUColorTargetState::format = %f",
                                 result->format);
    rnwgpu::Logger::logToConsole("GPUColorTargetState::blend = %f",
                                 result->blend);
    rnwgpu::Logger::logToConsole("GPUColorTargetState::writeMask = %f",
                                 result->writeMask);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::ColorTargetState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
