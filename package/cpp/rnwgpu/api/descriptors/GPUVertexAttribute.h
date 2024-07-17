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

struct GPUVertexAttribute {
  wgpu::VertexFormat format; // GPUVertexFormat
  double offset;             // GPUSize64
  double shaderLocation;     // GPUIndex32
};

static bool conv(wgpu::VertexAttribute &out,
                 std::shared_ptr<GPUVertexAttribute> &in) {

  out.format = in->format;
  return conv(out.offset, in->offset) &&
         conv(out.shaderLocation, in->shaderLocation);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexAttribute>> {
  static std::shared_ptr<rnwgpu::GPUVertexAttribute>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUVertexAttribute>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // format wgpu::VertexFormat
      // offset double
      // shaderLocation double
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexAttribute> arg) {
    throw std::runtime_error("Invalid GPUVertexAttribute::toJSI()");
  }
};

} // namespace margelo