#include "IOSPlatformContext.h"

#include "RNWebGPUManager.h"
#include "WebGPUModule.h"

namespace rnwgpu {

wgpu::Surface IOSPlatformContext::makeSurface(wgpu::Instance instance,
                                              void *surface, int width,
                                              int height) {
  wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
  metalSurfaceDesc.layer = surface;
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
  return instance.CreateSurface(&surfaceDescriptor);
}

} // namespace rnwgpu
