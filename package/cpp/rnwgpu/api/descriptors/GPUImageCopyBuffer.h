#pragma once

#include <memory>
#include <optional>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "DescriptorConvertors.h"
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

static bool conv(wgpu::ImageCopyBuffer &out,
                 const std::shared_ptr<GPUImageCopyBuffer> &in) {
  return conv(out.buffer, in->buffer) && conv(out.layout.offset, in->offset) &&
         conv(out.layout.bytesPerRow, in->bytesPerRow) &&
         conv(out.layout.rowsPerImage, in->rowsPerImage);
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
      if (value.hasProperty(runtime, "buffer")) {
        auto prop = value.getProperty(runtime, "buffer");
        result->buffer = JSIConverter<std::shared_ptr<GPUBuffer>>::fromJSI(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "offset")) {
        auto prop = value.getProperty(runtime, "offset");
        if (!prop.isUndefined()) {
          result->offset = JSIConverter<std::optional<double>>::fromJSI(
              runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "bytesPerRow")) {
        auto prop = value.getProperty(runtime, "bytesPerRow");
        if (!prop.isUndefined()) {
          result->bytesPerRow = JSIConverter<std::optional<double>>::fromJSI(
              runtime, prop, false);
        }
      }
      if (value.hasProperty(runtime, "rowsPerImage")) {
        auto prop = value.getProperty(runtime, "rowsPerImage");
        if (!prop.isUndefined()) {
          result->rowsPerImage = JSIConverter<std::optional<double>>::fromJSI(
              runtime, prop, false);
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageCopyBuffer> arg) {
    throw std::runtime_error("Invalid GPUImageCopyBuffer::toJSI()");
  }
};

} // namespace margelo