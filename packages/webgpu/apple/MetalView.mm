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
  // Retain the layer for as long as SurfaceInfo holds the pointer: the
  // latched attach (and the flush lambda that adopts it) can outlive this
  // view, e.g. across a dev reload where the registry is cleared before
  // dealloc runs. Balanced by the releaser below.
  void *nativeSurface = (void *)CFBridgingRetain(self.layer);
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto gpu = manager->_gpu;
  auto surface = manager->_platformContext->makeSurface(
      gpu, nativeSurface, size.width, size.height);
  // Find-or-create + attach runs atomically under the registry lock so a
  // concurrent destroyContext cannot orphan this surface.
  auto info = registry.attachSurface(
      [_contextId intValue], gpu, size.width, size.height, nativeSurface,
      surface, [](void *layer) {
        // The releaser can run on the rendering thread; CALayer teardown
        // belongs on the main thread.
        dispatch_async(dispatch_get_main_queue(), ^{
          CFBridgingRelease(layer);
        });
      });
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
