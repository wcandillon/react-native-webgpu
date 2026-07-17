#import "MetalView.h"
#import "webgpu/webgpu_cpp.h"

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
  if (manager == nullptr) {
    return;
  }
  void *nativeSurface = (__bridge void *)self.layer;
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto gpu = manager->_gpu;
  auto surface = manager->_platformContext->makeSurface(
      gpu, nativeSurface, size.width, size.height);
  auto info = registry.getSurfaceInfoOrCreate([_contextId intValue], gpu,
                                              size.width, size.height);
  // The layer pointer is unretained (the view owns the layer): no releaser.
  info->attachSurface(nativeSurface, surface, nullptr);
  // The attach is adopted at the next frame boundary by the rendering thread;
  // schedule a flush so contexts that are not currently rendering still pick
  // it up (and present their last offscreen frame).
  manager->flushPendingSurfaceTransition(info);
}

- (void)update {
  auto size = self.frame.size;
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo([_contextId intValue])) {
    info->resize(size.width, size.height);
  }
}

- (void)dealloc {
  // The view dies with its Canvas (contextIds are never reused), so view
  // teardown retires the registry entry. The JS-side cleanup
  // (RNWebGPU.destroyContext) only handles entries that never had a native
  // surface; see RNWebGPU::destroyContext for the ownership split.
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo([_contextId intValue])) {
    info->detachSurface();
  }
  registry.removeSurfaceInfo([_contextId intValue]);
}

@end
