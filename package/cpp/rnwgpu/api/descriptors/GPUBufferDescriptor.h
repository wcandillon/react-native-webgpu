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

struct GPUBufferDescriptor {
  double size;                          // GPUSize64
  double usage;                         // GPUBufferUsageFlags
  std::optional<bool> mappedAtCreation; // boolean
  std::optional<std::string> label;     // string
};

bool conv(wgpu::BufferDescriptor &out, GPUBufferDescriptor &in) {

  return conv(out.size, in.size) && conv(out.usage, in.usage) &&
         conv(out.mappedAtCreation, in.mappedAtCreation) &&
         conv(out.label, in.label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBufferDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBufferDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // size double
      // usage double
      // mappedAtCreation std::optional<bool>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferDescriptor> arg) {
    throw std::runtime_error("Invalid GPUBufferDescriptor::toJSI()");
  }
};

} // namespace margelo