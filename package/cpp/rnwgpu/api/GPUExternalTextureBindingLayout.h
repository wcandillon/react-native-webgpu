#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<wgpu::ExternalTextureBindingLayout>> {
  static std::shared_ptr<wgpu::ExternalTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::ExternalTextureBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<wgpu::ExternalTextureBindingLayout> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
