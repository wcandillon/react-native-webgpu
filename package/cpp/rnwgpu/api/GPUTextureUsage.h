#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUTextureUsage : public m::HybridObject {
public:
  GPUTextureUsage() : HybridObject("GPUTextureUsage") {}

public:
  wgpu::TextureUsage CopySrc() { return wgpu::TextureUsage::CopySrc; };
  wgpu::TextureUsage CopyDst() { return wgpu::TextureUsage::CopyDst; };
  wgpu::TextureUsage TextureBinding() {
    return wgpu::TextureUsage::TextureBinding;
  };
  wgpu::TextureUsage StorageBinding() {
    return wgpu::TextureUsage::StorageBinding;
  };
  wgpu::TextureUsage RenderAttachment() {
    return wgpu::TextureUsage::RenderAttachment;
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