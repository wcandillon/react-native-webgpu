#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUVertexAttribute.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUVertexBufferLayout {
  double arrayStride;                           // GPUSize64
  std::optional<wgpu::VertexStepMode> stepMode; // GPUVertexStepMode
  std::vector<std::shared_ptr<GPUVertexAttribute>>
      attributes; // Iterable<GPUVertexAttribute>
};

static bool conv(wgpu::VertexBufferLayout &out,
                 const std::shared_ptr<GPUVertexBufferLayout> &in) {
  return conv(out.attributes, out.attributeCount, in->attributes) &&
         conv(out.arrayStride, in->arrayStride) &&
         conv(out.stepMode, in->stepMode);
}
} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexBufferLayout>> {
  static std::shared_ptr<rnwgpu::GPUVertexBufferLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUVertexBufferLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // arrayStride double
      // stepMode std::optional<wgpu::VertexStepMode>
      // attributes std::vector<std::shared_ptr<GPUVertexAttribute>>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexBufferLayout> arg) {
    throw std::runtime_error("Invalid GPUVertexBufferLayout::toJSI()");
  }
};

} // namespace margelo