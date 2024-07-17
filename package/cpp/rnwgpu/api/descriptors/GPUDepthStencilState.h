#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "GPUStencilFaceState.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUDepthStencilState {
  wgpu::TextureFormat format;                        // GPUTextureFormat
  std::optional<bool> depthWriteEnabled;             // boolean
  std::optional<wgpu::CompareFunction> depthCompare; // GPUCompareFunction
  std::optional<std::shared_ptr<GPUStencilFaceState>>
      stencilFront; // GPUStencilFaceState
  std::optional<std::shared_ptr<GPUStencilFaceState>>
      stencilBack;                           // GPUStencilFaceState
  std::optional<double> stencilReadMask;     // GPUStencilValue
  std::optional<double> stencilWriteMask;    // GPUStencilValue
  std::optional<double> depthBias;           // GPUDepthBias
  std::optional<double> depthBiasSlopeScale; // number
  std::optional<double> depthBiasClamp;      // number
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDepthStencilState>> {
  static std::shared_ptr<rnwgpu::GPUDepthStencilState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDepthStencilState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "format")) {
        auto prop = value.getProperty(runtime, "format");
        result->format =
            JSIConverter::fromJSI<wgpu::TextureFormat>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthWriteEnabled")) {
        auto prop = value.getProperty(runtime, "depthWriteEnabled");
        result->depthWriteEnabled =
            JSIConverter::fromJSI<std::optional<bool>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthCompare")) {
        auto prop = value.getProperty(runtime, "depthCompare");
        result->depthCompare =
            JSIConverter::fromJSI<std::optional<wgpu::CompareFunction>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "stencilFront")) {
        auto prop = value.getProperty(runtime, "stencilFront");
        result->stencilFront = JSIConverter::fromJSI<
            std::optional<std::shared_ptr<GPUStencilFaceState>>>(runtime, prop,
                                                                 false);
      }
      if (value.hasProperty(runtime, "stencilBack")) {
        auto prop = value.getProperty(runtime, "stencilBack");
        result->stencilBack = JSIConverter::fromJSI<
            std::optional<std::shared_ptr<GPUStencilFaceState>>>(runtime, prop,
                                                                 false);
      }
      if (value.hasProperty(runtime, "stencilReadMask")) {
        auto prop = value.getProperty(runtime, "stencilReadMask");
        result->stencilReadMask =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "stencilWriteMask")) {
        auto prop = value.getProperty(runtime, "stencilWriteMask");
        result->stencilWriteMask =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthBias")) {
        auto prop = value.getProperty(runtime, "depthBias");
        result->depthBias =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthBiasSlopeScale")) {
        auto prop = value.getProperty(runtime, "depthBiasSlopeScale");
        result->depthBiasSlopeScale =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthBiasClamp")) {
        auto prop = value.getProperty(runtime, "depthBiasClamp");
        result->depthBiasClamp =
            JSIConverter::fromJSI<std::optional<double>>(runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUDepthStencilState> arg) {
    throw std::runtime_error("Invalid GPUDepthStencilState::toJSI()");
  }
};
} // namespace margelo