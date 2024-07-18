#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "DescriptorConvertors.h"
#include "GPUTextureView.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURenderPassDepthStencilAttachment {
  std::shared_ptr<GPUTextureView> view;        // GPUTextureView
  std::optional<double> depthClearValue;       // number
  std::optional<wgpu::LoadOp> depthLoadOp;     // GPULoadOp
  std::optional<wgpu::StoreOp> depthStoreOp;   // GPUStoreOp
  std::optional<bool> depthReadOnly;           // boolean
  std::optional<double> stencilClearValue;     // GPUStencilValue
  std::optional<wgpu::LoadOp> stencilLoadOp;   // GPULoadOp
  std::optional<wgpu::StoreOp> stencilStoreOp; // GPUStoreOp
  std::optional<bool> stencilReadOnly;         // boolean
};

static bool
conv(wgpu::RenderPassDepthStencilAttachment &out,
     const std::shared_ptr<GPURenderPassDepthStencilAttachment> &in) {
  return conv(out.view, in->view) &&
         conv(out.depthClearValue, in->depthClearValue) &&
         conv(out.depthLoadOp, in->depthLoadOp) &&
         conv(out.depthStoreOp, in->depthStoreOp) &&
         conv(out.depthReadOnly, in->depthReadOnly) &&
         conv(out.stencilClearValue, in->stencilClearValue) &&
         conv(out.stencilLoadOp, in->stencilLoadOp) &&
         conv(out.stencilStoreOp, in->stencilStoreOp) &&
         conv(out.stencilReadOnly, in->stencilReadOnly);
}
} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<
    std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result =
        std::make_unique<rnwgpu::GPURenderPassDepthStencilAttachment>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "view")) {
        auto prop = value.getProperty(runtime, "view");
        result->view = JSIConverter<std::shared_ptr<GPUTextureView>>::fromJSI(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthClearValue")) {
        auto prop = value.getProperty(runtime, "depthClearValue");
        if (!prop.isUndefined()) {
          result->depthClearValue =
              JSIConverter<std::optional<double>>::fromJSI(runtime, prop,
                                                           false);
        }
      }
      if (value.hasProperty(runtime, "depthLoadOp")) {
        auto prop = value.getProperty(runtime, "depthLoadOp");
        if (!prop.isUndefined()) {
          result->depthLoadOp =
              JSIConverter<std::optional<wgpu::LoadOp>>::fromJSI(runtime, prop,
                                                                 false);
        }
      }
      if (value.hasProperty(runtime, "depthStoreOp")) {
        auto prop = value.getProperty(runtime, "depthStoreOp");
        if (!prop.isUndefined()) {
          result->depthStoreOp =
              JSIConverter<std::optional<wgpu::StoreOp>>::fromJSI(runtime, prop,
                                                                  false);
        }
      }
      if (value.hasProperty(runtime, "depthReadOnly")) {
        auto prop = value.getProperty(runtime, "depthReadOnly");
        if (!prop.isUndefined()) {
          result->depthReadOnly =
              JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "stencilClearValue")) {
        auto prop = value.getProperty(runtime, "stencilClearValue");
        if (!prop.isUndefined()) {
          result->stencilClearValue =
              JSIConverter<std::optional<double>>::fromJSI(runtime, prop,
                                                           false);
        }
      }
      if (value.hasProperty(runtime, "stencilLoadOp")) {
        auto prop = value.getProperty(runtime, "stencilLoadOp");
        if (!prop.isUndefined()) {
          result->stencilLoadOp =
              JSIConverter<std::optional<wgpu::LoadOp>>::fromJSI(runtime, prop,
                                                                 false);
        }
      }
      if (value.hasProperty(runtime, "stencilStoreOp")) {
        auto prop = value.getProperty(runtime, "stencilStoreOp");
        if (!prop.isUndefined()) {
          result->stencilStoreOp =
              JSIConverter<std::optional<wgpu::StoreOp>>::fromJSI(runtime, prop,
                                                                  false);
        }
      }
      if (value.hasProperty(runtime, "stencilReadOnly")) {
        auto prop = value.getProperty(runtime, "stencilReadOnly");
        if (!prop.isUndefined()) {
          result->stencilReadOnly =
              JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
        }
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassDepthStencilAttachment> arg) {
    throw std::runtime_error(
        "Invalid GPURenderPassDepthStencilAttachment::toJSI()");
  }
};

} // namespace margelo