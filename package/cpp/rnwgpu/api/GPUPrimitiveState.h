#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::PrimitiveState>> {
  static std::shared_ptr<wgpu::PrimitiveState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::PrimitiveState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "topology")) {
        auto topology = value.getProperty(runtime, "topology");
      }
      if (value.hasProperty(runtime, "stripIndexFormat")) {
        auto stripIndexFormat = value.getProperty(runtime, "stripIndexFormat");
      }
      if (value.hasProperty(runtime, "frontFace")) {
        auto frontFace = value.getProperty(runtime, "frontFace");
      }
      if (value.hasProperty(runtime, "cullMode")) {
        auto cullMode = value.getProperty(runtime, "cullMode");
      }
      if (value.hasProperty(runtime, "unclippedDepth")) {
        auto unclippedDepth = value.getProperty(runtime, "unclippedDepth");
      }
    }
    rnwgpu::Logger::logToConsole("GPUPrimitiveState::topology = %f",
                                 result->topology);
    rnwgpu::Logger::logToConsole("GPUPrimitiveState::stripIndexFormat = %f",
                                 result->stripIndexFormat);
    rnwgpu::Logger::logToConsole("GPUPrimitiveState::frontFace = %f",
                                 result->frontFace);
    rnwgpu::Logger::logToConsole("GPUPrimitiveState::cullMode = %f",
                                 result->cullMode);
    rnwgpu::Logger::logToConsole("GPUPrimitiveState::unclippedDepth = %f",
                                 result->unclippedDepth);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::PrimitiveState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
