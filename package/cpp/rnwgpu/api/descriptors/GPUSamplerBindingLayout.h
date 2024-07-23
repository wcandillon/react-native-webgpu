#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"
#include "WGPULogger.h"

#include "RNFHybridObject.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUSamplerBindingLayout {
  std::optional<wgpu::SamplerBindingType> type; // GPUSamplerBindingType
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUSamplerBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUSamplerBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUSamplerBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "type")) {
        auto prop = value.getProperty(runtime, "type");
        result->type =
            JSIConverter<std::optional<wgpu::SamplerBindingType>>::fromJSI(
                runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUSamplerBindingLayout> arg) {
    throw std::runtime_error("Invalid GPUSamplerBindingLayout::toJSI()");
  }
};

} // namespace margelo