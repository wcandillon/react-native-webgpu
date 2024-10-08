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
  if (registry.hasSurface(contextId)) {
     auto info = registry.getSurface(contextId);
     auto surface = manager->_platformContext->makeSurface(info.gpu, nativeSurface, size.width, size.height);
     info.config.usage = info.config.usage | wgpu::TextureUsage::CopyDst;
     surface.Configure(&info.config);
     info.nativeSurface = nativeSurface;
     info.surface = surface;
     info.width = size.width;
     info.height = size.height;
     info.flush();
     surface.Present();
     registry.updateSurface(contextId, info);
   } else {
     // 2. The scene has not been drawn offscreen yet, we will draw onscreen directly
     rnwgpu::SurfaceInfo info;
     info.nativeSurface = nativeSurface;
     info.width = size.width;
     info.height = size.height;
     registry.addSurface(contextId, info);
   }
}

+ (void)updateSurface:(int)contextId size:(CGSize)size {
  // std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule
  // getManager];
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurface(contextId);
  info.width = size.width;
  info.height = size.height;
  registry.updateSurface(contextId, info);
}

@end
