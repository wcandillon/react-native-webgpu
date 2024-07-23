#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"
#include "WGPULogger.h"

#include "RNFHybridObject.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURequestAdapterOptions {
  std::optional<wgpu::PowerPreference> powerPreference; // GPUPowerPreference
  std::optional<bool> forceFallbackAdapter;             // boolean
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURequestAdapterOptions>> {
  static std::shared_ptr<rnwgpu::GPURequestAdapterOptions>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURequestAdapterOptions>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "powerPreference")) {
        auto prop = value.getProperty(runtime, "powerPreference");
        result->powerPreference =
            JSIConverter<std::optional<wgpu::PowerPreference>>::fromJSI(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "forceFallbackAdapter")) {
        auto prop = value.getProperty(runtime, "forceFallbackAdapter");
        result->forceFallbackAdapter =
            JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURequestAdapterOptions> arg) {
    throw std::runtime_error("Invalid GPURequestAdapterOptions::toJSI()");
  }
};

} // namespace margelo