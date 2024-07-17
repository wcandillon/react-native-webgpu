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

struct GPUMultisampleState {
  std::optional<double> count;                // GPUSize32
  std::optional<double> mask;                 // GPUSampleMask
  std::optional<bool> alphaToCoverageEnabled; // boolean
};

bool conv(wgpu::MultisampleState &out, GPUMultisampleState &in) {

  return conv(out.count, in.count) && conv(out.mask, in.mask) &&
         conv(out.alphaToCoverageEnabled, in.alphaToCoverageEnabled);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUMultisampleState>> {
  static std::shared_ptr<rnwgpu::GPUMultisampleState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUMultisampleState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // count std::optional<double>
      // mask std::optional<double>
      // alphaToCoverageEnabled std::optional<bool>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUMultisampleState> arg) {
    throw std::runtime_error("Invalid GPUMultisampleState::toJSI()");
  }
};

} // namespace margelo