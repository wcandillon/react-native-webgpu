#import "SurfaceUtils.h"
#import "MetalView.h"
#import "WebGPUModule.h"
#import "WebGPUView.h"

@implementation SurfaceUtils

+ (void)configureSurface:(CALayer *)layer
                    size:(CGSize)size
               contextId:(int)contextId {
  wgpu::SurfaceDescriptorFromMetalLayer metalSurfaceDesc;
  metalSurfaceDesc.layer = (void *)CFBridgingRetain(layer);
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  auto surfaceGpu = std::make_shared<wgpu::Surface>(
      manager->getGPU()->get().CreateSurface(&surfaceDescriptor));
  CGFloat scaleFactor = [UIScreen mainScreen].scale;
  float width = size.width * scaleFactor;
  float height = size.height * scaleFactor;
  rnwgpu::SurfaceData surfaceData = {width, height, surfaceGpu};
  manager->surfacesRegistry.addSurface(contextId, surfaceData);
}

@end
