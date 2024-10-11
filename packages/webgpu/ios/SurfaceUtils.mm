#import "SurfaceUtils.h"
#import "MetalView.h"
#import "WebGPUModule.h"
#import "WebGPUView.h"

// TODO: to delete now
@implementation SurfaceUtils

+ (void)configureSurface:(CALayer *)layer
                    size:(CGSize)size
               contextId:(int)contextId {
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  void *nativeSurface = (__bridge void *)layer;
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.createSurface(contextId, nativeSurface, size.width, size.height,
                         manager->_platformContext);
}

+ (void)updateSurface:(int)contextId size:(CGSize)size {
  // std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule
  // getManager];
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.setSize(contextId, size.width, size.height);
}

+ (void)cleanupSurface:(int)contextId {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  // Remove the surface info from the registry
  registry.removeSurface(contextId);
}

@end
