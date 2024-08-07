#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"
#include "RNWebGPUManager.h"

namespace rnwgpu {

class AndroidPlatformContext : public PlatformContext {
public:
  AndroidPlatformContext() = default;
  ~AndroidPlatformContext() = default;

  wgpu::Surface makeSurface(wgpu::Instance instance, void *window, int width, int height) override {
    wgpu::SurfaceDescriptorFromAndroidNativeWindow androidSurfaceDesc;
    androidSurfaceDesc.window = window;
    wgpu::SurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = &androidSurfaceDesc;
    return instance.CreateSurface(&surfaceDescriptor);
  }
};

} // namespace rnwgpu
