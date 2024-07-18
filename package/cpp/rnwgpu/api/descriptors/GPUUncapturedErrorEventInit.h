#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "DescriptorConvertors.h"
#include "GPUError.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUUncapturedErrorEventInit {
  std::shared_ptr<GPUError> error; // GPUError
  std::optional<bool> bubbles;     // boolean
  std::optional<bool> cancelable;  // boolean
  std::optional<bool> composed;    // boolean
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>> {
  static std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUUncapturedErrorEventInit>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "error")) {
        auto prop = value.getProperty(runtime, "error");
        result->error = JSIConverter<std::shared_ptr<GPUError>>::fromJSI(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "bubbles")) {
        auto prop = value.getProperty(runtime, "bubbles");
        result->bubbles =
            JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "cancelable")) {
        auto prop = value.getProperty(runtime, "cancelable");
        result->cancelable =
            JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "composed")) {
        auto prop = value.getProperty(runtime, "composed");
        result->composed =
            JSIConverter<std::optional<bool>>::fromJSI(runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit> arg) {
    throw std::runtime_error("Invalid GPUUncapturedErrorEventInit::toJSI()");
  }
};

} // namespace margelo