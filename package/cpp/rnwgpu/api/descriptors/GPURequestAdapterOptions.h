#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURequestAdapterOptions {
  std::optional<wgpu::PowerPreference> powerPreference; // GPUPowerPreference
  std::optional<bool> forceFallbackAdapter;             // boolean
};

bool conv(wgpu::RequestAdapterOptions &out,
          const GPURequestAdapterOptions &in) {

  return conv(out.powerPreference, in.powerPreference) &&
         conv(out.forceFallbackAdapter, in.forceFallbackAdapter);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURequestAdapterOptions>> {
  static std::shared_ptr<rnwgpu::GPURequestAdapterOptions>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURequestAdapterOptions>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // powerPreference std::optional<wgpu::PowerPreference>
      // forceFallbackAdapter std::optional<bool>
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