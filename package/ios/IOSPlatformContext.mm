#include "IOSPlatformContext.h"

#include "RNWebGPUManager.h"
#include "WebGPUModule.h"

namespace rnwgpu {

// TODO: are width and height in physical pixels?
wgpu::Surface IOSPlatformContext::makeSurface(void *surface, int width,
                                              int height) {
  wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
  metalSurfaceDesc.layer = surface;
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  return manager->getGPU()->get().CreateSurface(&surfaceDescriptor);
  // CGFloat scaleFactor = [UIScreen mainScreen].scale;
  // float width = size.width * scaleFactor;
  // float height = size.height * scaleFactor;
  // rnwgpu::SurfaceData surfaceData = {width, height, surfaceGpu};
}

} // namespace rnwgpu
