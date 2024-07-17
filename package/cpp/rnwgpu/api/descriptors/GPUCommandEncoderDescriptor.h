#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUCommandEncoderDescriptor {
  std::optional<std::string> label; // string
};

static bool conv(wgpu::CommandEncoderDescriptor &out,
                 std::shared_ptr<GPUCommandEncoderDescriptor> &in) {

  return conv(out.label, in->label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCommandEncoderDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUCommandEncoderDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUCommandEncoderDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // label std::optional<std::string>
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