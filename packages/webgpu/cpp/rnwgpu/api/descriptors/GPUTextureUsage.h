#pragma once
#include <string>

#include <RNFNativeObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;
namespace jsi = facebook::jsi;

class GPUTextureUsage : public m::NativeObject<GPUTextureUsage> {
public:
  static constexpr const char *CLASS_NAME = "GPUTextureUsage";

  GPUTextureUsage() : NativeObject(CLASS_NAME) {}

public:
  double CopySrc() { return static_cast<double>(wgpu::TextureUsage::CopySrc); }
  double CopyDst() { return static_cast<double>(wgpu::TextureUsage::CopyDst); }
  double TextureBinding() {
    return static_cast<double>(wgpu::TextureUsage::TextureBinding);
  }
  double StorageBinding() {
    return static_cast<double>(wgpu::TextureUsage::StorageBinding);
  }
  double RenderAttachment() {
    return static_cast<double>(wgpu::TextureUsage::RenderAttachment);
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "COPY_SRC", &GPUTextureUsage::CopySrc);
    installGetter(runtime, prototype, "COPY_DST", &GPUTextureUsage::CopyDst);
    installGetter(runtime, prototype, "TEXTURE_BINDING",
                  &GPUTextureUsage::TextureBinding);
    installGetter(runtime, prototype, "STORAGE_BINDING",
                  &GPUTextureUsage::StorageBinding);
    installGetter(runtime, prototype, "RENDER_ATTACHMENT",
                  &GPUTextureUsage::RenderAttachment);
  }
};
} // namespace rnwgpu
