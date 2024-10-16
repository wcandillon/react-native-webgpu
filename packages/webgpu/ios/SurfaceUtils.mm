#import "SurfaceUtils.h"
#import "MetalView.h"
#import "WebGPUModule.h"

@implementation SurfaceUtils

+ (void)configureSurface:(CALayer *)layer
                    size:(CGSize)size
               contextId:(int)contextId {
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  void *nativeSurface = (__bridge void *)layer;
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto gpu = manager->_gpu;
  auto surface = manager->_platformContext->makeSurface(
      gpu, nativeSurface, size.width, size.height);
  registry.getSurfaceInfoOrCreate(contextId, gpu, size.width, size.height)
      ->switchToOnscreen(nativeSurface, surface);
}

+ (void)updateSurface:(int)contextId size:(CGSize)size {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.getSurfaceInfo(contextId)->resize(size.width, size.height);
}

+ (void)cleanupSurface:(int)contextId {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  // Remove the surface info from the registry
  registry.removeSurfaceInfo(contextId);
}

@end
