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

struct GPUImageCopyBuffer {
  std::shared_ptr<GPUBuffer> buffer;  // GPUBuffer
  std::optional<double> offset;       // GPUSize64
  std::optional<double> bytesPerRow;  // GPUSize32
  std::optional<double> rowsPerImage; // GPUSize32
};

bool conv(wgpu::ImageCopyBuffer &out, const GPUImageCopyBuffer &in) {
  return conv(out.buffer, in.buffer) && conv(out.layout.offset, in.offset) &&
         conv(out.layout.bytesPerRow, in.bytesPerRow) &&
         conv(out.layout.rowsPerImage, in.rowsPerImage);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyBuffer>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyBuffer>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyBuffer>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // buffer std::shared_ptr<GPUBuffer>
      // offset std::optional<double>
      // bytesPerRow std::optional<double>
      // rowsPerImage std::optional<double>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageCopyBuffer> arg) {
    throw std::runtime_error("Invalid GPUImageCopyBuffer::toJSI()");
  }
};

} // namespace margelo