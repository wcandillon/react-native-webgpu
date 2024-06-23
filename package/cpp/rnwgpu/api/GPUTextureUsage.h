#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUTextureUsage : public m::HybridObject {
public:
  GPUTextureUsage() : HybridObject("GPUTextureUsage") {}

public:
  double CopySrc() { return static_cast<double>(wgpu::TextureUsage::CopySrc); };
  double CopyDst() { return static_cast<double>(wgpu::TextureUsage::CopyDst); };
  double TextureBinding() {
    return static_cast<double>(wgpu::TextureUsage::TextureBinding);
  };
  double StorageBinding() {
    return static_cast<double>(wgpu::TextureUsage::StorageBinding);
  };
  double RenderAttachment() {
    return static_cast<double>(wgpu::TextureUsage::RenderAttachment);
  }

  void loadHybridMethods() override {
    registerHybridGetter("COPY_SRC", &GPUTextureUsage::CopySrc, this);
    registerHybridGetter("COPY_DST", &GPUTextureUsage::CopyDst, this);
    registerHybridGetter("TEXTURE_BINDING", &GPUTextureUsage::TextureBinding,
                         this);
    registerHybridGetter("STORAGE_BINDING", &GPUTextureUsage::StorageBinding,
                         this);
    registerHybridGetter("RENDER_ATTACHMENT",
                         &GPUTextureUsage::RenderAttachment, this);
  }
};
} // namespace rnwgpu