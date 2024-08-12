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
  CGFloat scaleFactor = [UIScreen mainScreen].scale;
  void *nativeSurface = (__bridge void *)layer;
  float width = size.width * scaleFactor;
  float height = size.height * scaleFactor;
  rnwgpu::SurfaceData surfaceData = {width, height, nativeSurface};
  manager->surfacesRegistry.addSurface(contextId, surfaceData);
}

+ (void)updateSurface:(int)contextId size:(CGSize)size {
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  CGFloat scaleFactor = [UIScreen mainScreen].scale;
  float width = size.width * scaleFactor;
  float height = size.height * scaleFactor;
  manager->surfacesRegistry.updateSurface(contextId, width, height);
}

@end
