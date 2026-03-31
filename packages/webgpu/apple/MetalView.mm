#import "MetalView.h"
#import "webgpu/webgpu_cpp.h"

#include "AppleSurfaceBridge.h"
#include "SurfaceRegistry.h"

@implementation MetalView {
  BOOL _isConfigured;
  BOOL _isAttached;
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
  // Delay the configuration until we have a valid size
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  auto gpuWithLock = manager->_gpu;

  auto &registry = rnwgpu::SurfaceRegistry::getInstance();

  wgpu::SurfaceSourceMetalLayer metalSurfaceDesc;
  metalSurfaceDesc.layer = (__bridge void *)self.layer;
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &metalSurfaceDesc;
  // This is safe to call without holding a GPU lock
  wgpu::Surface surface = gpuWithLock.gpu.CreateSurface(&surfaceDescriptor);

  // Get or create the bridge.
  int ctxId = [_contextId intValue];

  // Create the bridge and attach the surface.
  // Safe to take the GPU lock here: prepareToDisplay runs on the UI thread
  // and never dispatch_sync's back to it, so no deadlock.
  auto bridge = std::static_pointer_cast<rnwgpu::AppleSurfaceBridge>(
      registry.getSurfaceInfoOrCreate(ctxId, gpuWithLock));

  void *nativeSurface = (__bridge void *)self.layer;
  bridge->prepareToDisplay(nativeSurface, surface);
}

- (void)update {
}

- (void)dealloc {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.removeSurfaceInfo([_contextId intValue]);
}

@end
