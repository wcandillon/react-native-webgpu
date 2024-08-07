#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"
#include "RNWebGPUManager.h"

namespace rnwgpu {

class AndroidPlatformContext : public PlatformContext {
public:
  AndroidPlatformContext(std::shared_ptr<rnwgpu::RNWebGPUManager> manager): _manager(manager) {}
  ~AndroidPlatformContext() = default;

  wgpu::Surface makeSurface(void* window, int width, int height) override {
    wgpu::SurfaceDescriptorFromAndroidNativeWindow androidSurfaceDesc;
    androidSurfaceDesc.window = window;
    wgpu::SurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = &androidSurfaceDesc;
    return _manager->getGPU()->get().CreateSurface(&surfaceDescriptor);
  }

private:
  std::shared_ptr<rnwgpu::RNWebGPUManager> _manager;
};

} // namespace rnwgpu