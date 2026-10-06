#import "MetalView.h"
#import "webgpu/webgpu_cpp.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>

#if !TARGET_OS_OSX
@interface MetalView ()
- (void)displayLinkDidFire;
@end

// CADisplayLink retains its target. Going through this proxy, which holds the
// view weakly, keeps a ticking link from keeping the view alive.
@interface RNWGDisplayLinkTarget : NSObject
@property(nonatomic, weak) MetalView *view;
- (void)tick:(CADisplayLink *)link;
@end

@implementation RNWGDisplayLinkTarget
- (void)tick:(CADisplayLink *)link {
  [self.view displayLinkDidFire];
}
@end
#endif // !TARGET_OS_OSX

@implementation MetalView {
  BOOL _isConfigured;
#if !TARGET_OS_OSX
  // Copy mode: frames are put on the layer from the main thread, at most one
  // per display refresh, by this link. It only ticks while frames keep coming.
  CADisplayLink *_displayLink;
  // Whether the link is ticking. Shared with the frame-ready callback, which
  // runs on the rendering thread and must not touch the view there.
  std::shared_ptr<std::atomic<bool>> _displayLinkActive;
  int _idleTicks;
  int _idleTicksBeforePause;
#endif
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

- (void)setPresentsWithCopy:(BOOL)presentsWithCopy {
  if (_presentsWithCopy == presentsWithCopy) {
    return;
  }
  _presentsWithCopy = presentsWithCopy;
#if !TARGET_OS_OSX
  if (!_isConfigured) {
    return; // -configure reads it
  }
  // Same layer, other way of presenting: detach (the canvas keeps its latest
  // frame offscreen) and attach again.
  [self detachKeepingContext];
  [self configure];
#endif
}

// Detach the layer from the canvas context without retiring the context.
- (void)detachKeepingContext {
#if !TARGET_OS_OSX
  [_displayLink invalidate];
  _displayLink = nil;
  // A wake-up already on its way to the main queue finds nothing to start.
  _displayLinkActive = nullptr;
#endif
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo([_contextId intValue])) {
    info->switchToOffscreen();
  }
  _isConfigured = NO;
}

- (void)configure {
  auto size = self.frame.size;
  std::shared_ptr<rnwgpu::RNWebGPUManager> manager = [WebGPUModule getManager];
  if (manager == nullptr) {
    return;
  }
  _isConfigured = YES;
  // Retain the layer for as long as SurfaceInfo holds the pointer: the
  // latched attach (and the flush lambda that adopts it) can outlive this
  // view, e.g. across a dev reload where the registry is cleared before
  // dealloc runs. Balanced by the releaser below.
  void *nativeSurface = (void *)CFBridgingRetain(self.layer);
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto gpu = manager->_gpu;
#if !TARGET_OS_OSX
  if (_presentsWithCopy) {
    [self configureCopyWithLayer:nativeSurface manager:manager];
    return;
  }
#endif
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

#if !TARGET_OS_OSX
// Copy mode: the canvas renders into textures the BlitPresenter owns, and this
// view copies the latest one onto its layer from the main thread. The layer's
// swapchain never leaves the main thread.
- (void)configureCopyWithLayer:(void *)nativeSurface
                       manager:
                           (std::shared_ptr<rnwgpu::RNWebGPUManager>)manager {
  auto size = self.frame.size;
  auto gpu = manager->_gpu;
  auto platformContext = manager->_platformContext;
  auto info = rnwgpu::SurfaceRegistry::getInstance().getSurfaceInfoOrCreate(
      [_contextId intValue], gpu, size.width, size.height);

  _displayLinkActive = std::make_shared<std::atomic<bool>>(false);
  auto active = _displayLinkActive;
  __weak MetalView *weakSelf = self;
  info->blitPresenter().setFrameReadyCallback([weakSelf, active]() {
    // Rendering thread. A ticking link picks the frame up by itself;
    // otherwise wake it up. The view is only resolved on the main thread, so
    // its last reference is never dropped here.
    if (active->load()) {
      return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf startDisplayLink];
    });
  });

  rnwgpu::BlitPresenter::NativeSurface surface;
  surface.create = [gpu, platformContext, nativeSurface]() {
    return platformContext->makeSurface(gpu, nativeSurface, 0, 0);
  };
  surface.configured = [nativeSurface](wgpu::TextureFormat format) {
    rnwgpu::applyCAMetalLayerColorSpace(nativeSurface, format);
  };
  surface.release = [nativeSurface]() {
    // Can run on the rendering thread; CALayer teardown belongs on the main
    // thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      CFBridgingRelease(nativeSurface);
    });
  };
  info->enableBlit(size.width, size.height, std::move(surface));
  // Show the latest frame right away, if the canvas already rendered one.
  [self startDisplayLink];
}

- (void)startDisplayLink {
  if (_displayLinkActive == nullptr) {
    return;
  }
  if (_displayLink == nil) {
    RNWGDisplayLinkTarget *target = [RNWGDisplayLinkTarget new];
    target.view = self;
    _displayLink = [CADisplayLink displayLinkWithTarget:target
                                               selector:@selector(tick:)];
    NSInteger maxFps = UIScreen.mainScreen.maximumFramesPerSecond;
    if (@available(iOS 15.0, tvOS 15.0, *)) {
      // Follow the display up to its fastest refresh rate (the system still
      // caps it according to the app's settings).
      _displayLink.preferredFrameRateRange =
          CAFrameRateRangeMake(30, maxFps, maxFps);
    }
    // About a quarter of a second without a new frame.
    _idleTicksBeforePause = std::max<int>(2, static_cast<int>(maxFps / 4));
    [_displayLink addToRunLoop:NSRunLoop.mainRunLoop
                       forMode:NSRunLoopCommonModes];
  }
  _idleTicks = 0;
  _displayLinkActive->store(true);
  _displayLink.paused = NO;
}

// One present per display refresh at most: acquiring a drawable faster than
// the display consumes them would block the main thread.
- (void)displayLinkDidFire {
  auto info = rnwgpu::SurfaceRegistry::getInstance().getSurfaceInfo(
      [_contextId intValue]);
  if (info != nullptr && info->blitPresenter().presentFrame()) {
    _idleTicks = 0;
    return;
  }
  if (info != nullptr && ++_idleTicks < _idleTicksBeforePause) {
    return;
  }
  // No frame for a while: stop ticking until the next one is ready.
  _displayLink.paused = YES;
  _displayLinkActive->store(false);
  // A frame that finished just before the flag flipped found the link
  // ticking and did not wake it: look once more.
  if (info != nullptr && info->blitPresenter().presentFrame()) {
    [self startDisplayLink];
  }
}
#endif // !TARGET_OS_OSX

- (void)update {
  auto size = self.frame.size;
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo([_contextId intValue])) {
    info->resize(size.width, size.height);
  }
}

- (void)dealloc {
#if !TARGET_OS_OSX
  [_displayLink invalidate];
#endif
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
