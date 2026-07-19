#import "MetalView.h"
#include "SurfaceRegistry.h"
#import "webgpu/webgpu_cpp.h"

#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
#include <utility>

namespace {

rnwgpu::SurfaceOwnerId nextMetalViewOwnerId() noexcept {
  static std::atomic<rnwgpu::SurfaceOwnerId> nextOwnerId{1};
  for (;;) {
    const auto ownerId = nextOwnerId.fetch_add(1, std::memory_order_relaxed);
    if (ownerId != rnwgpu::kInvalidSurfaceOwnerId) {
      return ownerId;
    }
  }
}

std::shared_ptr<void> retainMetalLayer(CAMetalLayer *layer) {
  if (layer == nil) {
    return {};
  }

  void *retainedLayer = (__bridge_retained void *)layer;
  return std::shared_ptr<void>(retainedLayer, [](void *value) noexcept {
    if ([NSThread isMainThread]) {
      CFRelease(value);
      return;
    }

    // A Canvas can be released by a worklet. Keep the final Objective-C
    // release on the UI thread even though retain/release themselves are
    // thread-safe, because this may be the CAMetalLayer's last owner.
    dispatch_async(dispatch_get_main_queue(), ^{
      CFRelease(value);
    });
  });
}

void detachSurface(rnwgpu::SurfaceOwnerId ownerId,
                   rnwgpu::RNWebGPUSessionId &sessionId, int &contextId,
                   std::shared_ptr<rnwgpu::SurfaceInfo> &surfaceInfo) noexcept {
  auto detachedSurface = std::move(surfaceInfo);
  const auto detachedSessionId =
      std::exchange(sessionId, rnwgpu::kInvalidRNWebGPUSessionId);
  const auto detachedContextId = std::exchange(contextId, 0);
  if (!detachedSurface) {
    return;
  }

  try {
    (void)rnwgpu::SurfaceRegistry::getInstance().removeSurfaceInfoIfOwnedBy(
        detachedSessionId, detachedContextId, ownerId, detachedSurface);
  } catch (...) {
    // Cleanup must not escape view recycling or deallocation.
  }
}

} // namespace

@implementation MetalView {
  rnwgpu::SurfaceOwnerId _surfaceOwnerId;
  rnwgpu::RNWebGPUSessionId _configuredSessionId;
  int _configuredContextId;
  std::shared_ptr<rnwgpu::SurfaceInfo> _surfaceInfo;
}

#if !TARGET_OS_OSX
+ (Class)layerClass {
  return [CAMetalLayer class];
}
#endif // !TARGET_OS_OSX

- (instancetype)init {
  self = [super init];
  if (self) {
    _surfaceOwnerId = nextMetalViewOwnerId();
#if TARGET_OS_OSX
    self.wantsLayer = true;
    self.layer = [CAMetalLayer layer];
#endif
  }
  return self;
}

- (void)configure {
  if (_surfaceOwnerId == rnwgpu::kInvalidSurfaceOwnerId) {
    _surfaceOwnerId = nextMetalViewOwnerId();
  }
  detachSurface(_surfaceOwnerId, _configuredSessionId, _configuredContextId,
                _surfaceInfo);

  if (_sessionId == nil || _contextId == nil || self.layer == nil) {
    return;
  }

  const auto sessionValue = [_sessionId doubleValue];
  const auto contextId = [_contextId intValue];
  if (!std::isfinite(sessionValue) || sessionValue < 1.0 ||
      sessionValue > static_cast<double>(rnwgpu::kMaxRNWebGPUSessionId) ||
      sessionValue != std::trunc(sessionValue) || contextId <= 0) {
    return;
  }
  const auto sessionId = static_cast<rnwgpu::RNWebGPUSessionId>(sessionValue);

  try {
    auto &managerRegistry = rnwgpu::RNWebGPUManagerRegistry::getInstance();
    auto managerSnapshot = managerRegistry.get(sessionId);
    if (!managerSnapshot || !managerSnapshot.manager->isActive()) {
      return;
    }
    auto manager = std::move(managerSnapshot.manager);

    const auto size = self.frame.size;
    const auto width = static_cast<int>(size.width);
    const auto height = static_cast<int>(size.height);
    auto *metalLayer = (CAMetalLayer *)self.layer;
    void *nativeSurface = (__bridge void *)metalLayer;
    auto nativeSurfaceOwner = retainMetalLayer(metalLayer);
    auto gpu = manager->_gpu;

    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto surfaceInfo = registry.claimSurfaceInfo(
        sessionId, contextId, _surfaceOwnerId, gpu, width, height);
    if (!surfaceInfo) {
      return;
    }

    _configuredSessionId = sessionId;
    _configuredContextId = contextId;
    _surfaceInfo = std::move(surfaceInfo);

    auto surface = manager->_platformContext->makeSurface(gpu, nativeSurface,
                                                          width, height);
    if (!manager->isActive() ||
        !_surfaceInfo->attachSurfaceIfOwnedBy(_surfaceOwnerId, nativeSurface,
                                              std::move(surface),
                                              std::move(nativeSurfaceOwner))) {
      detachSurface(_surfaceOwnerId, _configuredSessionId, _configuredContextId,
                    _surfaceInfo);
      return;
    }
    // Surface adoption is latched to a frame boundary. Flush on the JS thread
    // too so a context with static/offscreen content is republished promptly.
    manager->flushPendingSurfaceTransition(_surfaceInfo);
  } catch (const std::exception &error) {
    detachSurface(_surfaceOwnerId, _configuredSessionId, _configuredContextId,
                  _surfaceInfo);
    NSLog(@"Failed to configure react-native-webgpu surface: %s", error.what());
  } catch (...) {
    detachSurface(_surfaceOwnerId, _configuredSessionId, _configuredContextId,
                  _surfaceInfo);
    NSLog(@"Failed to configure react-native-webgpu surface: unknown native "
           "error");
  }
}

- (void)update {
  if (!_surfaceInfo ||
      _configuredSessionId == rnwgpu::kInvalidRNWebGPUSessionId) {
    return;
  }

  auto manager =
      rnwgpu::RNWebGPUManagerRegistry::getInstance().get(_configuredSessionId);
  if (!manager || !manager.manager->isActive()) {
    return;
  }

  auto ownedSurface =
      rnwgpu::SurfaceRegistry::getInstance().getSurfaceInfoIfOwnedBy(
          _configuredSessionId, _configuredContextId, _surfaceOwnerId);
  if (!ownedSurface || ownedSurface != _surfaceInfo) {
    return;
  }

  const auto size = self.frame.size;
  ownedSurface->resizeIfOwnedBy(_surfaceOwnerId, static_cast<int>(size.width),
                                static_cast<int>(size.height));
}

- (void)dealloc {
  detachSurface(_surfaceOwnerId, _configuredSessionId, _configuredContextId,
                _surfaceInfo);
}

@end
