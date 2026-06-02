#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "SurfaceRegistry.h"
#include "rnwgpu/async/RuntimeScheduler.h"

namespace rnwgpu {

/**
 * Global vsync-driven auto-present coordinator. Replaces the manual
 * `context.present()` call.
 *
 * Flow:
 *   - `GPUCanvasContext::getCurrentTexture()` (JS thread) calls
 * `requestPresent` for its surface, tagged with the owning runtime's
 * RuntimeScheduler.
 *   - A platform vsync source (iOS CADisplayLink / Android Choreographer) calls
 *     `onVSync()` on the UI thread once per frame.
 *   - On each vsync, every surface that requested a present has its present
 *     dispatched onto its owning runtime's JS thread (so `Surface.Present()`
 * and the Apple Metal scheduling wait run on the same thread that did
 *     getCurrentTexture / submit, preserving Dawn surface thread-affinity and
 *     present-after-submit ordering via FIFO on that loop).
 *
 * The vsync source is request-driven: it is started when the first present is
 * requested and stopped after a few idle frames, so an idle (non-rendering) app
 * costs zero CPU.
 */
class FrameDriver {
public:
  static FrameDriver &getInstance();

  /**
   * Register how to start/stop the platform vsync source. `start`/`stop` are
   * invoked when presents begin/cease; each implementation is responsible for
   * hopping to the UI thread as needed. Called once per platform at init.
   */
  void setPlatformVSync(std::function<void()> start,
                        std::function<void()> stop);

  /**
   * Request that `surface` be presented at the next vsync. Coalesced per
   * contextId (at most one present per surface per frame). Thread-safe; called
   * from a JS thread inside getCurrentTexture. Surfaces with no on-screen
   * `wgpu::Surface` (offscreen) should not be registered.
   */
  void requestPresent(int contextId, std::shared_ptr<SurfaceInfo> surface,
                      std::shared_ptr<async::RuntimeScheduler> scheduler);

  /**
   * Drop any pending present for a surface (e.g. when its view is torn down).
   * Thread-safe.
   */
  void cancelPresent(int contextId);

  /** Called by the platform vsync source on the UI thread, once per frame. */
  void onVSync();

private:
  FrameDriver() = default;

  struct Pending {
    std::shared_ptr<SurfaceInfo> surface;
    std::shared_ptr<async::RuntimeScheduler> scheduler;
  };

  // Number of consecutive empty frames before the vsync source is stopped.
  // A small grace period avoids start/stop thrash during continuous rendering.
  static constexpr int kMaxIdleFrames = 3;

  std::mutex _mutex;
  std::unordered_map<int, Pending> _pending;
  std::function<void()> _start;
  std::function<void()> _stop;
  bool _running = false;
  int _idleFrames = 0;
};

} // namespace rnwgpu
