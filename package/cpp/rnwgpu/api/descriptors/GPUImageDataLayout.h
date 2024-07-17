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

struct GPUImageDataLayout {
  std::optional<double> offset;       // GPUSize64
  std::optional<double> bytesPerRow;  // GPUSize32
  std::optional<double> rowsPerImage; // GPUSize32
};

bool conv(wgpu::ImageDataLayout &out, GPUImageDataLayout &in) {

  return conv(out.offset, in.offset) && conv(out.bytesPerRow, in.bytesPerRow) &&
         conv(out.rowsPerImage, in.rowsPerImage);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageDataLayout>> {
  static std::shared_ptr<rnwgpu::GPUImageDataLayout>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageDataLayout>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // offset std::optional<double>
      // bytesPerRow std::optional<double>
      // rowsPerImage std::optional<double>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageDataLayout> arg) {
    throw std::runtime_error("Invalid GPUImageDataLayout::toJSI()");
  }
};

} // namespace margelo