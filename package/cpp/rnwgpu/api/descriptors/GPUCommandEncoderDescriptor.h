#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"
#include "WGPULogger.h"

#include "RNFHybridObject.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUCommandEncoderDescriptor {
  std::optional<std::string> label; // string
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCommandEncoderDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUCommandEncoderDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUCommandEncoderDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter<std::optional<std::string>>::fromJSI(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUCommandEncoderDescriptor> arg) {
    throw std::runtime_error("Invalid GPUCommandEncoderDescriptor::toJSI()");
  }
};

} // namespace margelo