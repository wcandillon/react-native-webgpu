#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPURequestAdapterOptions {
public:
  wgpu::RequestAdapterOptions *getInstance() { return &_instance; }

  wgpu::RequestAdapterOptions _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURequestAdapterOptions>> {
  static std::shared_ptr<rnwgpu::GPURequestAdapterOptions>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURequestAdapterOptions>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "powerPreference")) {
        auto powerPreference = value.getProperty(runtime, "powerPreference");

        if (powerPreference.isString()) {
          auto str = powerPreference.asString(runtime).utf8(runtime);
          wgpu::PowerPreference enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.powerPreference = enumValue;
        }
      }
      if (value.hasProperty(runtime, "forceFallbackAdapter")) {
        auto forceFallbackAdapter =
            value.getProperty(runtime, "forceFallbackAdapter");
        if (forceFallbackAdapter.isBool()) {
          result->_instance.forceFallbackAdapter =
              forceFallbackAdapter.getBool();
        }
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURequestAdapterOptions> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
