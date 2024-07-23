#pragma once

#include "RNFHybridObject.h"

namespace rnwgpu {

struct GPUSurfaceConfiguration {
  std::shared_ptr<GPUDevice> device;
  wgpu::TextureFormat format;
//  TextureUsage usage = TextureUsage::RenderAttachment;
  size_t viewFormatCount = 0;
//  TextureFormat const * viewFormats;
//  CompositeAlphaMode alphaMode = CompositeAlphaMode::Auto;
  uint32_t width;
  uint32_t height;
//  PresentMode presentMode = PresentMode::Fifo;
};

} // namespace rnwgpu

namespace margelo {

  using namespace rnwgpu; // NOLINT(build/namespaces)

  template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUSurfaceConfiguration>> {
    static std::shared_ptr<rnwgpu::GPUSurfaceConfiguration>
    fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
      auto result = std::make_unique<rnwgpu::GPUSurfaceConfiguration>();
      return result;
    }
    static jsi::Value toJSI(jsi::Runtime &runtime,
                            std::shared_ptr<rnwgpu::GPUSurfaceConfiguration> arg) {
      throw std::runtime_error("Invalid GPUVertexAttribute::toJSI()");
    }
  };

} // namespace margelo
