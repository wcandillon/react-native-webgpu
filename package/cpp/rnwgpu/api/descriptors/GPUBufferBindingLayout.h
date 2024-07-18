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

struct GPUBufferBindingLayout {
  std::optional<wgpu::BufferBindingType> type; // GPUBufferBindingType
  std::optional<bool> hasDynamicOffset;        // boolean
  std::optional<double> minBindingSize;        // GPUSize64
};

static bool conv(wgpu::BufferBindingLayout &out,
                 const std::shared_ptr<GPUBufferBindingLayout> &in) {
  return conv(out.type, in->type) &&
         conv(out.hasDynamicOffset, in->hasDynamicOffset) &&
         conv(out.minBindingSize, in->minBindingSize);
}
} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferBindingLayout>> {
  static std::shared_ptr<rnwgpu::GPUBufferBindingLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBufferBindingLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // type std::optional<wgpu::BufferBindingType>
      // hasDynamicOffset std::optional<bool>
      // minBindingSize std::optional<double>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferBindingLayout> arg) {
    throw std::runtime_error("Invalid GPUBufferBindingLayout::toJSI()");
  }
};

} // namespace margelo