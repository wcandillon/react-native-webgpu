#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
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

bool conv(wgpu::UncapturedErrorEventInit &out,
          const GPUUncapturedErrorEventInit &in) {

  return conv(out.error, in.error) && conv(out.bubbles, in.bubbles) &&
         conv(out.cancelable, in.cancelable) && conv(out.composed, in.composed);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>> {
  static std::shared_ptr<rnwgpu::GPUUncapturedErrorEventInit>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUUncapturedErrorEventInit>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // error std::shared_ptr<GPUError>
      // bubbles std::optional<bool>
      // cancelable std::optional<bool>
      // composed std::optional<bool>
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