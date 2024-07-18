#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "DescriptorConvertors.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBlendComponent {
  std::optional<wgpu::BlendOperation> operation; // GPUBlendOperation
  std::optional<wgpu::BlendFactor> srcFactor;    // GPUBlendFactor
  std::optional<wgpu::BlendFactor> dstFactor;    // GPUBlendFactor
};

static bool conv(wgpu::BlendComponent &out,
                 const std::shared_ptr<GPUBlendComponent> &in) {
  return conv(out.operation, in->operation) &&
         conv(out.srcFactor, in->srcFactor) &&
         conv(out.dstFactor, in->dstFactor);
}
} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBlendComponent>> {
  static std::shared_ptr<rnwgpu::GPUBlendComponent>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBlendComponent>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "operation")) {
        auto prop = value.getProperty(runtime, "operation");
        if (!prop.isUndefined()) {
          result->operation =
              JSIConverter<std::optional<wgpu::BlendOperation>>::fromJSI(
                  runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "srcFactor")) {
        auto prop = value.getProperty(runtime, "srcFactor");
        if (!prop.isUndefined()) {
          result->srcFactor =
              JSIConverter<std::optional<wgpu::BlendFactor>>::fromJSI(
                  runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "dstFactor")) {
        auto prop = value.getProperty(runtime, "dstFactor");
        if (!prop.isUndefined()) {
          result->dstFactor =
              JSIConverter<std::optional<wgpu::BlendFactor>>::fromJSI(
                  runtime, prop, false);
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendComponent> arg) {
    throw std::runtime_error("Invalid GPUBlendComponent::toJSI()");
  }
};

} // namespace margelo