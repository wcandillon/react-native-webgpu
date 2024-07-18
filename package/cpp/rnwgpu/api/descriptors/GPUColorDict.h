#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUColorDict {
  double r; // number
  double g; // number
  double b; // number
  double a; // number
};

static bool conv(wgpu::Color &out, const std::shared_ptr<GPUColorDict> &in) {

  return conv(out.r, in->r) && conv(out.g, in->g) && conv(out.b, in->b) &&
         conv(out.a, in->a);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUColorDict>> {
  static std::shared_ptr<rnwgpu::GPUColorDict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUColorDict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // r double
      // g double
      // b double
      // a double
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUColorDict> arg) {
    throw std::runtime_error("Invalid GPUColorDict::toJSI()");
  }
};

} // namespace margelo