#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "DescriptorConvertors.h"
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

static bool conv(wgpu::DepthStencilState &out,
                 const std::shared_ptr<GPUDepthStencilState> &in) {
  return conv(out.format, in->format) &&
         conv(out.depthWriteEnabled, in->depthWriteEnabled) &&
         conv(out.depthCompare, in->depthCompare) &&
         conv(out.stencilFront, in->stencilFront) &&
         conv(out.stencilBack, in->stencilBack) &&
         conv(out.stencilReadMask, in->stencilReadMask) &&
         conv(out.stencilWriteMask, in->stencilWriteMask) &&
         conv(out.depthBias, in->depthBias) &&
         conv(out.depthBiasSlopeScale, in->depthBiasSlopeScale) &&
         conv(out.depthBiasClamp, in->depthBiasClamp);
}
} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDepthStencilState>> {
  static std::shared_ptr<rnwgpu::GPUDepthStencilState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDepthStencilState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "format")) {
        auto prop = value.getProperty(runtime, "format");
        result->format =
            JSIConverter<wgpu::TextureFormat>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthWriteEnabled")) {
        auto prop = value.getProperty(runtime, "depthWriteEnabled");
        if (!prop.isUndefined()) {
          result->depthWriteEnabled =
              JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "depthCompare")) {
        auto prop = value.getProperty(runtime, "depthCompare");
        if (!prop.isUndefined()) {
          result->depthCompare =
              JSIConverter<std::optional<wgpu::CompareFunction>>::fromJSI(
                  runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "stencilFront")) {
        auto prop = value.getProperty(runtime, "stencilFront");
        if (!prop.isUndefined()) {
          result->stencilFront = JSIConverter<std::optional<
              std::shared_ptr<GPUStencilFaceState>>>::fromJSI(runtime, prop,
                                                              false);
        }
      }
      if (value.hasProperty(runtime, "stencilBack")) {
        auto prop = value.getProperty(runtime, "stencilBack");
        if (!prop.isUndefined()) {
          result->stencilBack = JSIConverter<std::optional<
              std::shared_ptr<GPUStencilFaceState>>>::fromJSI(runtime, prop,
                                                              false);
        }
      }
      if (value.hasProperty(runtime, "stencilReadMask")) {
        auto prop = value.getProperty(runtime, "stencilReadMask");
        if (!prop.isUndefined()) {
          result->stencilReadMask =
              JSIConverter<std::optional<double>>::fromJSI(runtime, prop,
                                                           false);
        }
      }
      if (value.hasProperty(runtime, "stencilWriteMask")) {
        auto prop = value.getProperty(runtime, "stencilWriteMask");
        if (!prop.isUndefined()) {
          result->stencilWriteMask =
              JSIConverter<std::optional<double>>::fromJSI(runtime, prop,
                                                           false);
        }
      }
      if (value.hasProperty(runtime, "depthBias")) {
        auto prop = value.getProperty(runtime, "depthBias");
        if (!prop.isUndefined()) {
          result->depthBias = JSIConverter<std::optional<double>>::fromJSI(
              runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "depthBiasSlopeScale")) {
        auto prop = value.getProperty(runtime, "depthBiasSlopeScale");
        if (!prop.isUndefined()) {
          result->depthBiasSlopeScale =
              JSIConverter<std::optional<double>>::fromJSI(runtime, prop,
                                                           false);
        }
      }
      if (value.hasProperty(runtime, "depthBiasClamp")) {
        auto prop = value.getProperty(runtime, "depthBiasClamp");
        if (!prop.isUndefined()) {
          result->depthBiasClamp = JSIConverter<std::optional<double>>::fromJSI(
              runtime, prop, false);
        }
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