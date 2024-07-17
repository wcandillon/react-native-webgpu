#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUBuffer.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBufferBinding {
  std::shared_ptr<GPUBuffer> buffer; // GPUBuffer
  std::optional<double> offset;      // GPUSize64
  std::optional<double> size;        // GPUSize64
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferBinding>> {
  static std::shared_ptr<rnwgpu::GPUBufferBinding>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBufferBinding>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // buffer std::shared_ptr<GPUBuffer>
      // offset std::optional<double>
      // size std::optional<double>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferBinding> arg) {
    throw std::runtime_error("Invalid GPUBufferBinding::toJSI()");
  }
};

} // namespace margelo