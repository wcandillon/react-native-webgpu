#import "MetalView.h"
#import "webgpu_cpp.h"

@implementation MetalView {
  BOOL _isConfigured;
}

#if !TARGET_OS_OSX
+ (Class)layerClass {
  return [CAMetalLayer class];
}
#else  // !TARGET_OS_OSX
- (instancetype)init {
  self = [super init];
  if (self) {
    self.wantsLayer = true;
    self.layer = [CAMetalLayer layer];
  }
  return self;
}
#endif // !TARGET_OS_OSX

- (void)configure {
  auto size = self.frame.size;
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  void *nativeSurface = (__bridge void *)self.layer;
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto gpu = manager->_gpu;
  auto surface = manager->_platformContext->makeSurface(
      gpu, nativeSurface, size.width, size.height);
  registry
      .getSurfaceInfoOrCreate([_contextId intValue], gpu, size.width,
                              size.height)
      ->switchToOnscreen(nativeSurface, surface);
}

- (void)update {
  auto size = self.frame.size;
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.getSurfaceInfo([_contextId intValue])
      ->resize(size.width, size.height);
}

- (void)dealloc {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  // Remove the surface info from the registry
  registry.removeSurfaceInfo([_contextId intValue]);
}

@end
