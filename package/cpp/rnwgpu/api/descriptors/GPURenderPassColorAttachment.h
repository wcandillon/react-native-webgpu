#pragma once

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUColorDict.h"
#include "GPUTextureView.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURenderPassColorAttachment {
  std::shared_ptr<GPUTextureView> view; // GPUTextureView
  std::optional<double> depthSlice;     // GPUIntegerCoordinate
  std::optional<std::shared_ptr<GPUTextureView>>
      resolveTarget; // GPUTextureView
  std::optional<
      std::variant<std::vector<double>, std::shared_ptr<GPUColorDict>>>
      clearValue;        // GPUColor
  wgpu::LoadOp loadOp;   // GPULoadOp
  wgpu::StoreOp storeOp; // GPUStoreOp
};

bool conv(wgpu::RenderPassColorAttachment &out,
          const GPURenderPassColorAttachment &in) {
  return conv(out.view, in.view) && conv(out.depthSlice, in.depthSlice) &&
         conv(out.resolveTarget, in.resolveTarget) &&
         conv(out.clearValue, in.clearValue) && conv(out.loadOp, in.loadOp) &&
         conv(out.storeOp, in.storeOp);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>> {
  static std::shared_ptr<rnwgpu::GPURenderPassColorAttachment>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassColorAttachment>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "view")) {
        auto prop = value.getProperty(runtime, "view");
        result->view = JSIConverter<std::shared_ptr<GPUTextureView>>::fromJSI(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "depthSlice")) {
        auto prop = value.getProperty(runtime, "depthSlice");
        result->depthSlice =
            JSIConverter<std::optional<double>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "resolveTarget")) {
        auto prop = value.getProperty(runtime, "resolveTarget");
        result->resolveTarget = JSIConverter<
            std::optional<std::shared_ptr<GPUTextureView>>>::fromJSI(runtime,
                                                                     prop,
                                                                     false);
      }
      if (value.hasProperty(runtime, "clearValue")) {
        auto prop = value.getProperty(runtime, "clearValue");
        result->clearValue = JSIConverter<std::optional<
            std::variant<std::vector<double>, std::shared_ptr<GPUColorDict>>>>::
            fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "loadOp")) {
        auto prop = value.getProperty(runtime, "loadOp");
        result->loadOp =
            JSIConverter<wgpu::LoadOp>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "storeOp")) {
        auto prop = value.getProperty(runtime, "storeOp");
        result->storeOp =
            JSIConverter<wgpu::StoreOp>::fromJSI(runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassColorAttachment> arg) {
    throw std::runtime_error("Invalid GPURenderPassColorAttachment::toJSI()");
  }
};
} // namespace margelo