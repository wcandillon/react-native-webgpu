#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"
#include "WGPULogger.h"

#include "RNFHybridObject.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUExternalTextureBindingLayout {};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExternalTextureBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUExternalTextureBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUExternalTextureBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUExternalTextureBindingLayout> arg) {
    throw std::runtime_error(
        "Invalid GPUExternalTextureBindingLayout::toJSI()");
  }
};

} // namespace margelo