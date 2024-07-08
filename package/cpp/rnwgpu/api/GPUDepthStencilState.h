#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::DepthStencilState>> {
  static std::shared_ptr<wgpu::DepthStencilState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::DepthStencilState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUDepthStencilState::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUDepthStencilState::format is not defined");
      }
      if (value.hasProperty(runtime, "depthWriteEnabled")) {
        auto depthWriteEnabled =
            value.getProperty(runtime, "depthWriteEnabled");
      }
      if (value.hasProperty(runtime, "depthCompare")) {
        auto depthCompare = value.getProperty(runtime, "depthCompare");
      }
      if (value.hasProperty(runtime, "stencilFront")) {
        auto stencilFront = value.getProperty(runtime, "stencilFront");
      }
      if (value.hasProperty(runtime, "stencilBack")) {
        auto stencilBack = value.getProperty(runtime, "stencilBack");
      }
      if (value.hasProperty(runtime, "stencilReadMask")) {
        auto stencilReadMask = value.getProperty(runtime, "stencilReadMask");

        if (stencilReadMask.isNumber()) {
          result->stencilReadMask =
              static_cast<wgpu::StencilValue>(stencilReadMask.getNumber());
        }
      }
      if (value.hasProperty(runtime, "stencilWriteMask")) {
        auto stencilWriteMask = value.getProperty(runtime, "stencilWriteMask");

        if (stencilWriteMask.isNumber()) {
          result->stencilWriteMask =
              static_cast<wgpu::StencilValue>(stencilWriteMask.getNumber());
        }
      }
      if (value.hasProperty(runtime, "depthBias")) {
        auto depthBias = value.getProperty(runtime, "depthBias");

        if (depthBias.isNumber()) {
          result->depthBias =
              static_cast<wgpu::DepthBias>(depthBias.getNumber());
        }
      }
      if (value.hasProperty(runtime, "depthBiasSlopeScale")) {
        auto depthBiasSlopeScale =
            value.getProperty(runtime, "depthBiasSlopeScale");

        if (depthBiasSlopeScale.isNumber()) {
          result->depthBiasSlopeScale = depthBiasSlopeScale.getNumber();
        }
      }
      if (value.hasProperty(runtime, "depthBiasClamp")) {
        auto depthBiasClamp = value.getProperty(runtime, "depthBiasClamp");

        if (depthBiasClamp.isNumber()) {
          result->depthBiasClamp = depthBiasClamp.getNumber();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::format = %f",
                                 result->format);
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::depthWriteEnabled = %f",
                                 result->depthWriteEnabled);
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::depthCompare = %f",
                                 result->depthCompare);
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::stencilFront = %f",
                                 result->stencilFront);
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::stencilBack = %f",
                                 result->stencilBack);
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::stencilReadMask = %f",
                                 result->stencilReadMask);
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::stencilWriteMask = %f",
                                 result->stencilWriteMask);
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::depthBias = %f",
                                 result->depthBias);
    rnwgpu::Logger::logToConsole(
        "GPUDepthStencilState::depthBiasSlopeScale = %f",
        result->depthBiasSlopeScale);
    rnwgpu::Logger::logToConsole("GPUDepthStencilState::depthBiasClamp = %f",
                                 result->depthBiasClamp);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::DepthStencilState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
