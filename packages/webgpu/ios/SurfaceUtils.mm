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
  manager->surfacesRegistry.addSurface(contextId, nativeSurface, size.width,
                                       size.height);
}

+ (void)updateSurface:(int)contextId size:(CGSize)size {
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  manager->surfacesRegistry.updateSurface(contextId, size.width, size.height);
}

@end
