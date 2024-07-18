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

struct GPUCommandBufferDescriptor {
  std::optional<std::string> label; // string
};

static bool conv(wgpu::CommandBufferDescriptor &out,
                 const std::shared_ptr<GPUCommandBufferDescriptor> &in) {

  return conv(out.label, in->label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCommandBufferDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUCommandBufferDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUCommandBufferDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUCommandBufferDescriptor> arg) {
    throw std::runtime_error("Invalid GPUCommandBufferDescriptor::toJSI()");
  }
};

} // namespace margelo