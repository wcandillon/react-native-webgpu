#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "CanvasColorSpace.h"
#include "PlatformContext.h"
#include "WebGPURuntime.h"
#include "rnsurface/FrameScheduler.h"
#include "rnsurface/ThreadChecker.h"

#if defined(__ANDROID__)
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <poll.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
namespace dawn::native::metal {
void WaitForCommandsToBeScheduled(WGPUDevice device);
} // namespace dawn::native::metal
#endif

namespace rnwgpu {

#ifdef __APPLE__
// Tags the CAMetalLayer with the colorspace matching the configured texture
// format and the canvas's colorSpace, and opts it into the display's extended
// dynamic range for an "extended" toneMapping mode. Implemented in
// apple/MetalLayerColorSpace.mm.
void applyCAMetalLayerColorSpace(void *nativeSurface,
                                 wgpu::TextureFormat format,
                                 CanvasColorSpace colorSpace,
                                 CanvasToneMappingMode toneMappingMode);
#endif

struct NativeInfo {
  void *nativeSurface;
  int width;
  int height;
};

struct Size {
  int width;
  int height;
};

// Invoked with the platform's native surface pointer once SurfaceInfo is done
// with it, so the platform can drop the reference it acquired on our behalf
// (ANativeWindow_release on Android, CFBridgingRelease of the retained
// CAMetalLayer on Apple platforms). May run on any thread.
using NativeSurfaceReleaser = std::function<void(void *)>;

#if defined(__ANDROID__)
// --- AHB-pool presentation mode (WebGPUNativeBufferView)
// -----------------------------
//
// A third presentation backend (besides the on-screen wgpu::Surface swapchain
// and the offscreen texture). WebGPU renders into a small pool of native
// AHardwareBuffers imported as Dawn SharedTextureMemory; the view draws each
// finished buffer inline via Bitmap.wrapHardwareBuffer so it behaves like a
// normal RN view. The pool is sized from the JS canvas drawing buffer
// (_canvas->getWidth()/Height()) and reallocated when that changes, exactly
// like the swapchain, so the canvas texture always matches the app's other
// attachments (e.g. its depth texture).

#define RNWGPU_LOG_TAG "WebGPUNativeBufferView"
// Failure / anomaly reporting only (never per-frame).
#define RNWGPU_POOL_WARN(...)                                                  \
  __android_log_print(ANDROID_LOG_WARN, RNWGPU_LOG_TAG, __VA_ARGS__)

enum class SlotState : uint8_t {
  Free,      // available for poolGetCurrentTexture
  Rendering, // BeginAccess done, JS rendering into it
  Presented, // EndAccess done, fence(s) queued for the waiter
  Ready,     // fence signaled, awaiting UI pickup
  Displayed, // handed to the consumer, held until released (held-ring)
};

struct PoolSlot {
  AHardwareBuffer *ahb = nullptr; // owned by the pool (allocated natively)
  wgpu::SharedTextureMemory memory = nullptr;
  wgpu::Texture texture = nullptr;
  SlotState state = SlotState::Free;
};

// One generation of the pool (one buffer size). Reference-counted (shared_ptr)
// so an in-flight render / present / ready slot keeps the whole generation
// alive across a resize. The destructor frees every AHB and tears down the Dawn
// imports. The Java side keeps its own ref (via wrapHardwareBuffer) for
// anything it is still drawing, so a generation can be freed here without
// disturbing a frame still on screen.
struct AHBPool {
  std::vector<PoolSlot> slots;
  uint32_t generation = 0;
  int width = 0;
  int height = 0;

  ~AHBPool() {
    for (auto &slot : slots) {
      slot.texture = nullptr;
      slot.memory = nullptr;
      if (slot.ahb != nullptr) {
        AHardwareBuffer_release(slot.ahb);
        slot.ahb = nullptr;
      }
    }
  }
};
#endif

// The native side of a canvas: what the render runtime draws into, the window
// the platform view presents it on, and when. A canvas presents in one of two
// ways (see Presentation), chosen as its Canvas mounts.
//
// Direct, the default: getCurrentTexture() acquires the window's own texture
// and present() presents it, as wgpu and Metal present. No copy, the usage the
// canvas was configured with, and the window's own pacing: the window hands
// out its next texture once the previous one went to it, so the next
// getCurrentTexture() waits for the present in flight (see presentDirect).
// The present runs on the present thread (below) when the device
// synchronizes itself (see configure), so that a render runtime on the main
// thread or the JS thread does not pay for it; on the render runtime's thread
// otherwise.
//
// Composited: the render runtime never touches the window. getCurrentTexture()
// hands it one of a few offscreen textures (the frames, see Frame) and
// present() marks that frame ready and asks the present thread for a frame,
// which the frame scheduler (rnsurface::FrameScheduler, shared with React
// Native Skia) decides when to present: on the next vsync for a window that
// shows each frame in turn (a SurfaceView, a CAMetalLayer), once the window
// drew the previous one for a window that shows the latest frame presented (a
// TextureView). The present thread then acquires the window's texture, copies
// the frame onto it and presents (see presentFrame). The render runtime never
// waits, every frame it presents reaches a TextureView, and the window shows
// the last frame again when it comes back. React Native Skia's canvases
// present this way.
//
// The window belongs to the present thread of the platform in both cases, one
// for every canvas of a process (PresentThread.java, RNWGPresentThread in
// WebGPUView.mm), where the platform view posts the attach and the detach
// from the main thread. Everything that touches the Dawn surface runs there,
// one call after the other, which the entries check once the platform view
// has bound the thread (see bindPresentThread and rnsurface::ThreadChecker),
// except what a direct canvas's render runtime does to it under the surface's
// lock: the acquire, the configuration, and the present when the device does
// not synchronize itself. Nothing on the present thread waits for a present.
// A present holds the thread for as long as the window takes to hand out a
// texture and take it back, which on Android waits for the GPU (libgui's
// queueBuffer waits on the previous buffer's fence) and on a TextureView of
// Android 11 for the window's draw, seconds at a time; a render runtime made
// to wait for that skipped frames, and a main thread made to wait for it is
// an ANR.
//
// Frames (composited): at most kMaxFrames offscreen textures, each in one
// state. The render runtime draws into the one it is rendering (kRendering)
// and present() makes it the ready one (kReady; an older ready frame it
// replaces is let go of: the window shows the latest frame, as a TextureView
// does). The present thread takes the ready frame (kPresenting), copies it
// onto the window's texture and keeps it as the frame the window shows
// (kShown), the one presented again when the window comes back (a view out of
// its window keeps its frame, as a CAMetalLayer's contents and a web canvas's
// drawing buffer do). The copy is submitted to the device's queue, which runs
// it before anything the render runtime submits into the same texture
// afterwards, so a frame is the render runtime's again as soon as its copy is
// submitted. The render runtime never waits for a frame: one is free, or the
// shown one is taken over.
//
// A device without Dawn's implicit device synchronization may only be used
// from the thread of its runtime, which the present thread is not: a
// composited canvas configured with one is refused (see configure), and a
// direct one presents on the render runtime's thread.
//
// Locks, innermost last: the registry's, _surfaceMutex (the Dawn surface, held
// by the present thread for the whole of a present, and by the thread that
// destroys a device, see releaseSurfaceForDevice), _mutex (the frames, the
// configuration, the layout size: short sections on every thread). The pool's
// _poolMutex is never nested under _mutex while blocking.
//
// A context presenting to a native window of the caller's
// (MakeWebGPUCanvasContextFromNativeWindow) has no view: it owns its window
// (see the constructor), presents direct, and the caller drives it from one
// thread at a time.
//
// The registry entry is created on first use (by whichever of JS/native gets
// there first) and lives exactly as long as its JS Canvas: contextIds are
// never reused, not even across a reload (see allocateContextId). It is
// removed by the native view's teardown (WebGPUView's invalidate /
// WebGPUViewManager.onDropViewInstance) when a view attached, or by
// RNWebGPU.destroyContext (the Canvas unmount cleanup) when none ever did,
// see RNWebGPU::destroyContext for why the split. Surface destruction alone
// (backgrounding, TextureView teardown) never removes an entry; it only
// detaches the window.
class SurfaceInfo : public std::enable_shared_from_this<SurfaceInfo> {
public:
  using Window = rnsurface::FrameScheduler::Window;
  using PresentResult = rnsurface::FrameScheduler::PresentResult;

  // How the canvas presents (see the class comment): the Canvas's
  // presentation prop. Direct unless asked otherwise.
  enum class Presentation { kDirect, kComposited };

  // How many offscreen frames a composited canvas holds at most: the one
  // being rendered, the one ready for the window, and the one the window
  // shows.
  static constexpr size_t kMaxFrames = 3;

  // ownsWindow is for a surface on a window the caller handed in (see
  // SurfaceRegistry::makeUnregisteredSurfaceInfo), which no view attaches or
  // detaches. unconfigure() then destroys the Dawn surface rather than only
  // unconfiguring it, and configure() makes a new one for the same window. On
  // Android, a window keeps the buffers it handed to the swapchain until the
  // VkSurfaceKHR is destroyed, which Surface::Unconfigure does not do (it
  // parks the swapchain for the next Configure): a MediaCodec input surface
  // would otherwise keep them until the context is garbage collected. The
  // window itself is retained until the SurfaceInfo is destroyed.
  SurfaceInfo(wgpu::Instance gpu, int width, int height,
              bool ownsWindow = false)
      : _gpu(std::move(gpu)), _width(width), _height(height),
        _ownsWindow(ownsWindow), _host(*this), _scheduler(_host) {}

  SurfaceInfo(const SurfaceInfo &) = delete;
  SurfaceInfo &operator=(const SurfaceInfo &) = delete;

  ~SurfaceInfo() {
#if defined(__ANDROID__)
    // Stop the fence waiter before any pool generation is freed.
    stopWaiter();
    {
      std::lock_guard<std::mutex> poolLock(_poolMutex);
      _presentQueue.clear();
      poolResetLocked();
      _poolDevice = nullptr;
    }
#endif
    // Wherever the last owner lets go of the canvas: the Dawn objects go
    // before the native window they borrow.
    std::lock_guard<std::mutex> surfaceLock(_surfaceMutex);
    releaseWindowLocked();
    std::unique_lock<std::shared_mutex> lock(_mutex);
    for (auto &frame : _frames) {
      frame = Frame();
    }
    _fallback = nullptr;
    _current = nullptr;
  }

  // --- Platform view, main thread ------------------------------------------

  // Whether a view attached a window, as the main thread posted it: set as
  // the attach is posted to the present thread, cleared as the detach is.
  // What hasNativeSurface() answers until the posts have run, so that a
  // context destroyed between the two (React's StrictMode runs the Canvas
  // cleanup right after mount) leaves the entry to the view (see
  // RNWebGPU::destroyContext).
  void setViewAttached(bool attached) {
    _viewAttached.store(attached, std::memory_order_release);
  }

  // How the canvas presents (see Presentation), from the Canvas's prop: the
  // platform view passes it as it claims the canvas
  // (SurfaceRegistry::attachView) and the JS context as it is made
  // (RNWebGPU::MakeWebGPUCanvasContext), whichever comes first. Taken while
  // the canvas is not configured, which is before the first frame: a
  // configured canvas keeps the presentation it was configured with, and a
  // canvas that changes it is remounted. Any thread.
  void setPresentation(Presentation presentation) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      _presentation.store(presentation, std::memory_order_relaxed);
    }
  }

  Presentation presentation() const {
    return _presentation.load(std::memory_order_relaxed);
  }

  // The view's layout size in dp (surfaced as clientWidth/clientHeight on the
  // JS canvas). Does not resize the drawing buffer: that tracks
  // canvas.width/height (like on the web), see GPUCanvasContext. Any thread.
  void setLayoutSize(int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _width = width;
    _height = height;
  }

  // The platform is about to take the window back, and will not wait for
  // detach(): a SurfaceView's surface is destroyed as soon as surfaceDestroyed
  // returns, and the main thread waits for the detach posted to the present
  // thread only so long (see WebGPUView.onSurfaceDestroyed). From now until
  // detach() runs, nothing is presented onto the window: a present in flight
  // skips its present, as presenting onto a window that is gone loses the
  // GPU device. Called before that detach is posted, so that it comes before
  // the check the present thread makes between copying a frame and
  // presenting it. Only detach() takes it down: an attach() under way when it
  // is set keeps it, as the detach posted behind that attach is what it is
  // for. Any thread.
  void abandon() { _abandoned.store(true, std::memory_order_release); }

  // --- Present thread -------------------------------------------------------

  // Binds the entries that belong to the present thread (see the class
  // comment) to the calling thread, before the first of them: the platform
  // view calls it on the present thread ahead of attach(). A context without
  // a view (MakeWebGPUCanvasContextFromNativeWindow) never binds one.
  void bindPresentThread() { _presentThread.bindToCurrentThread(); }

  // What the scheduler asks of the platform view: a frame callback on the
  // next vsync (the Choreographer of the present thread on Android, the
  // display link on Apple platforms), which calls onFrame(). Installed
  // before attach(), on the present thread.
  void setPostFrameCallback(std::function<void()> postFrameCallback) {
    _postFrameCallback = std::move(postFrameCallback);
  }

  // Told after each frame presentFrame() shows on the window; the copy of the
  // last frame onto a window that came back does not count. The view uses it
  // to vote the display's refresh rate while the canvas animates (see
  // SurfaceFrameRateVote.java). Present thread; nullptr to unregister.
  void setFramePresented(std::function<void()> framePresented) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _framePresented = std::move(framePresented);
  }

  // Installed by the platform view: whether the window can show a frame right
  // now (not while the app is in the background on Apple platforms, where GPU
  // work is refused). Nothing is presented while it returns false: a
  // composited canvas's frame waits for onCanPresent(), a direct canvas
  // renders into an offscreen texture meanwhile (see
  // getCurrentTextureDirect). Present thread; read under _mutex by the
  // render runtime.
  void setCanPresent(std::function<bool()> canPresent) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _canPresent = std::move(canPresent);
  }

  // Configures the native window (an ANativeWindow or a CAMetalLayer) as the
  // Dawn surface of this canvas. A window already attached is detached first.
  // `window` says how the backing view shows the frames presented to it (see
  // rnsurface::FrameScheduler::Window). The releaser gives the handle back
  // when the window is detached, or now when no surface can be made over it,
  // in which case this returns false. A composited canvas presents the last
  // frame it showed, if any, onto the new window on the next vsync; a direct
  // one configures the window on its next acquire. Present thread, or the
  // calling thread for a context without a view.
  bool attach(void *nativeSurface, NativeSurfaceReleaser releaser,
              Window window) {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::attach");
    std::unique_lock<std::mutex> surfaceLock(_surfaceMutex);
    // Not detach(): an abandon that lands while this runs (the platform taking
    // the window back as it attaches) must be kept for the detach posted
    // behind, not taken down here.
    releaseWindowLocked();
    wgpu::Surface surface = WebGPURuntime::makeSurface(_gpu, nativeSurface);
    if (surface == nullptr) {
      if (releaser) {
        releaser(nativeSurface);
      }
      return false;
    }
    _surface = std::move(surface);
    const bool composited = presentation() == Presentation::kComposited;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _nativeSurface = nativeSurface;
      _releaser = std::move(releaser);
      _needsConfigure = true;
      // A new window shows nothing until a frame goes to it.
      _showLastFrame = composited;
    }
    if (composited) {
      // Outside the surface's lock: a window that shows the latest frame
      // presents on the spot (see rnsurface::FrameScheduler::requestFrame).
      surfaceLock.unlock();
      _scheduler.setWindow(window);
      _scheduler.requestFrame();
    }
    return true;
  }

  // Destroys the Dawn surface and gives the native handle back to its
  // releaser: the platform may destroy the window right after. The frames
  // stay: the next window shows the last one. Takes down the abandon, if any
  // (see abandon): this is the detach it waits for. Present thread; the
  // destructor does the same on whichever thread lets go of the canvas last.
  void detach() {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::detach");
    detachNow();
  }

  // detach(), if the window attached is still `nativeSurface`. An Apple view
  // lets go of its layer this way, as it leaves its superview: the detach is
  // posted behind the present in flight, and an attach posted after it, for
  // another layer, stays. Present thread.
  void detach(void *nativeSurface) {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::detach");
    bool attached;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      attached = nativeSurface != nullptr && nativeSurface == _nativeSurface;
    }
    if (attached) {
      detachNow();
    }
  }

  // Whether a window is attached. Present thread.
  bool isAttached() {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::isAttached");
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _nativeSurface != nullptr;
  }

  // A recording of the render runtime is ready (see present): the scheduler
  // decides when it goes to the window. Present thread.
  void scheduleFrame() {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::scheduleFrame");
    _scheduler.requestFrame();
  }

  // The vsync the scheduler asked for (see setPostFrameCallback). Present
  // thread.
  void onFrame() {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::onFrame");
    _scheduler.onFrame();
  }

  // The TextureView drew the frame it was given: the next one can go to it
  // (see rnsurface::FrameScheduler::onFrameConsumed). Present thread.
  void onFrameConsumed() {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::onFrameConsumed");
    _scheduler.onFrameConsumed();
  }

  // The view left its window, or the window lost its surface, and the
  // platform view removed its callbacks: the scheduler forgets what it asked
  // for. Present thread.
  void cancelScheduledFrames() {
    RNSURFACE_ASSERT_THREAD(_presentThread,
                            "SurfaceInfo::cancelScheduledFrames");
    _scheduler.cancel();
  }

  // Whether a frame callback is outstanding: a host that ticks on its own (a
  // display link) pauses when none is. Present thread.
  bool isFrameCallbackPosted() const {
    return _scheduler.isFrameCallbackPosted();
  }

  // The window can show frames again (the app came back to the foreground,
  // see setCanPresent): the frame that waited goes on the next vsync. Present
  // thread.
  void onCanPresent() {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::onCanPresent");
    bool waiting;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      waiting = indexOf(FrameState::kReady) >= 0 || _showLastFrame;
    }
    if (waiting) {
      _scheduler.requestFrame();
    }
  }

  // Presents the ready frame onto the window, as the scheduler decides (see
  // the class comment): configures the surface when the window or the
  // drawing buffer changed since, acquires its texture, copies the frame onto
  // it and presents. Says whether a frame went to the window, and whether
  // one is left for a later frame: a frame the window could not take (no
  // texture, see kMoreQueued) is tried again on the next vsync. Without a
  // window, a configuration or a device, while the app cannot show a frame
  // (see setCanPresent) or once the platform took the window back (see
  // abandon), the frame waits. Present thread.
  int presentFrame() {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::presentFrame");
    std::lock_guard<std::mutex> surfaceLock(_surfaceMutex);
    int index = -1;
    bool fromShown = false;
    bool configure = false;
    wgpu::Device device;
    WindowConfiguration config;
    wgpu::Texture source;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_surface == nullptr || _config.device == nullptr ||
          _abandoned.load(std::memory_order_acquire) ||
          (_canPresent && !_canPresent())) {
        return PresentResult::kNothingPresented;
      }
      index = indexOf(FrameState::kReady);
      if (index < 0 && _showLastFrame) {
        index = indexOf(FrameState::kShown);
        fromShown = true;
      }
      _showLastFrame = false;
      if (index < 0) {
        return PresentResult::kNothingPresented;
      }
      Frame &frame = _frames[index];
      if (!matchesConfigLocked(frame.texture)) {
        // A frame for a drawing buffer size the canvas no longer has: the
        // render runtime has moved on, and the window takes the next one.
        frame = Frame();
        return PresentResult::kNothingPresented;
      }
      frame.state = FrameState::kPresenting;
      source = frame.texture;
      device = _config.device;
      config = windowConfigurationLocked(/* direct = */ false);
      configure = _needsConfigure;
      _needsConfigure = false;
    }
    if (configure) {
      configureSurfaceLocked(config);
    }
    wgpu::Texture destination =
        _surface ? acquireSurfaceTextureLocked(config) : nullptr;
    if (destination == nullptr) {
      // No texture this frame (the window is mid-resize, or lost while the
      // app was in the background): the frame waits for the next vsync.
      std::unique_lock<std::shared_mutex> lock(_mutex);
      Frame &frame = _frames[index];
      frame.state = fromShown ? FrameState::kShown : FrameState::kReady;
      _showLastFrame = fromShown;
      _needsConfigure = true;
      return PresentResult::kMoreQueued;
    }
    copyTexture(device, source, destination);
    std::function<void()> framePresented;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      // The copy is submitted: the frame is the render runtime's again, as
      // the one the window shows.
      const int shown = indexOf(FrameState::kShown);
      if (shown >= 0 && shown != index) {
        _frames[shown].state = FrameState::kFree;
      }
      _frames[index].state = FrameState::kShown;
      framePresented = fromShown ? nullptr : _framePresented;
    }
    // The platform took the window back while the frame was copied (see
    // abandon): the texture is not presented, as the window is going.
    if (_abandoned.load(std::memory_order_acquire)) {
      return PresentResult::kNothingPresented;
    }
#ifdef __APPLE__
    // Ensure the copy's command buffer is scheduled before presenting.
    dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
#endif
    _surface.Present();
    if (framePresented) {
      framePresented();
    }
    bool more;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      more = indexOf(FrameState::kReady) >= 0;
    }
    return PresentResult::kPresented |
           (more ? PresentResult::kMoreQueued
                 : PresentResult::kNothingPresented);
  }

  // --- Render runtime -------------------------------------------------------

  // Whether the device may be used from a thread other than its runtime's
  // (Dawn's implicit device synchronization), which a present on the present
  // thread is.
  static bool synchronizes(const wgpu::Device &device) {
    return device != nullptr &&
           device.HasFeature(wgpu::FeatureName::ImplicitDeviceSynchronization);
  }

  void configure(
      wgpu::SurfaceConfiguration &newConfig,
      std::vector<wgpu::TextureFormat> viewFormats,
      CanvasColorSpace colorSpace = CanvasColorSpace::SRGB,
      CanvasToneMappingMode toneMappingMode = CanvasToneMappingMode::Standard) {
    const bool direct = presentation() == Presentation::kDirect;
    const bool synchronized = synchronizes(newConfig.device);
    if (!direct && newConfig.device != nullptr && !synchronized) {
      // The present thread copies the frames onto the window on this device
      // (see the class comment), which only its runtime's thread may use.
      throw std::runtime_error(
          "[WebGPU] a composited canvas needs a device with implicit device "
          "synchronization (the default): its frames are presented from a "
          "thread of their own. Drop implicitDeviceSynchronization: false "
          "from the device descriptor, or present direct (the default)");
    }
    // The surface is the present thread's; only a direct canvas touches it
    // here, once the present in flight is done with it.
    std::unique_lock<std::mutex> surfaceLock(_surfaceMutex, std::defer_lock);
    if (direct) {
      waitForPendingPresent();
      surfaceLock.lock();
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _deviceSynchronized = synchronized;
    _viewFormats = std::move(viewFormats);
    _colorSpace = colorSpace;
    _toneMappingMode = toneMappingMode;
    _config = newConfig;
    // The caller's viewFormats storage dies with the call; point the stored
    // configuration at our own copy.
    _config.viewFormats = _viewFormats.empty() ? nullptr : _viewFormats.data();
    _config.viewFormatCount = _viewFormats.size();
    // The drawing buffer starts at the canvas size. Clamp so a canvas that has
    // not been laid out yet (0x0) configures instead of erroring.
    _config.width = std::max(1, _width);
    _config.height = std::max(1, _height);
    _config.presentMode = wgpu::PresentMode::Fifo;
    // The frames were made for the previous configuration.
    dropFramesLocked();
    _needsConfigure = true;
#if defined(__ANDROID__)
    {
      std::lock_guard<std::mutex> poolLock(_poolMutex);
      if (_poolDevice.Get() != _config.device.Get()) {
        // Buffers imported for another device cannot be reused.
        poolResetLocked();
      }
      _poolDevice = _config.device;
      _poolFormat = _config.format;
      _poolUsage = _config.usage;
      // A new device may support AHB sharing even if the previous one did not.
      _poolUnsupported = false;
    }
#endif
    if (direct) {
      if (_surface == nullptr && _nativeSurface != nullptr) {
        // unconfigure() destroyed the surface (see the constructor), or a
        // device destroyed took it (see releaseSurfaceForDevice): make a new
        // one for the window.
        recreateSurfaceLocked(_nativeSurface);
      }
      // Not onto a window the platform is taking back (see abandon): the
      // acquire configures the window that comes next.
      if (_surface && !_abandoned.load(std::memory_order_acquire)) {
        configureSurfaceLocked(windowConfigurationLocked(/* direct = */ true));
        _needsConfigure = false;
      }
    }
  }

  // Resize the drawing buffer (canvas.width/height changed).
  void reconfigure(int newWidth, int newHeight) {
    const bool direct = presentation() == Presentation::kDirect;
    std::unique_lock<std::mutex> surfaceLock(_surfaceMutex, std::defer_lock);
    if (direct) {
      waitForPendingPresent();
      surfaceLock.lock();
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      return;
    }
    _config.width = std::max(1, newWidth);
    _config.height = std::max(1, newHeight);
    // A frame being rendered at the previous size never presented: the next
    // acquire supersedes it.
    const int rendering = indexOf(FrameState::kRendering);
    if (rendering >= 0) {
      _frames[rendering] = Frame();
    }
    _fallback = nullptr;
    _current = nullptr;
    _frameInFlight = false;
    _frameEpoch++;
    _needsConfigure = true;
    if (direct && _surface && !_abandoned.load(std::memory_order_acquire)) {
      configureSurfaceLocked(windowConfigurationLocked(/* direct = */ true));
      _needsConfigure = false;
    }
  }

  void unconfigure() {
    const bool direct = presentation() == Presentation::kDirect;
    std::unique_lock<std::mutex> surfaceLock(_surfaceMutex, std::defer_lock);
    if (direct) {
      waitForPendingPresent();
      surfaceLock.lock();
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_ownsWindow && _surface) {
      // Destroy the surface, and so its swapchain, while their device is
      // still alive. The window is kept: configure() makes a new surface for
      // it (see the constructor).
      _surface = nullptr;
#if defined(__ANDROID__)
      // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
      _surfaceDevice = nullptr;
#endif
    }
    // A viewed canvas leaves its surface configured: the next configure()
    // configures it again, and a device destroyed meanwhile takes it with it
    // (see releaseSurfaceForDevice).
    dropFramesLocked();
    _config = {};
    _colorSpace = CanvasColorSpace::SRGB;
    _toneMappingMode = CanvasToneMappingMode::Standard;
    _viewFormats.clear();
    _needsConfigure = false;
#if defined(__ANDROID__)
    {
      std::lock_guard<std::mutex> poolLock(_poolMutex);
      poolResetLocked();
      _poolDevice = nullptr;
    }
#endif
  }

  bool isConfigured() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _config.device != nullptr;
  }

  // Returns the texture for the current frame: the surface's own texture for
  // a direct canvas with a window that can take a frame (see
  // getCurrentTextureDirect), an offscreen frame otherwise (see the class
  // comment). Never returns null; throws when called before configure().
  // When frameEpoch is given, it receives the epoch identifying this frame
  // (see isCurrentFrame).
  wgpu::Texture getCurrentTexture(uint64_t *frameEpoch = nullptr) {
#if defined(__ANDROID__)
    if (_poolMode.load()) {
      // The pool tracks the drawing buffer (_config.width/height, kept in sync
      // with canvas.width/height by reconfigure()), like the swapchain, so the
      // canvas texture always matches the app's other attachments.
      int poolW = 0;
      int poolH = 0;
      {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        if (_config.device != nullptr) {
          poolW = static_cast<int>(_config.width);
          poolH = static_cast<int>(_config.height);
        }
      }
      poolResize(poolW, poolH);
      if (auto texture = poolGetCurrentTexture()) {
        std::unique_lock<std::shared_mutex> lock(_mutex);
        _frameInFlight = true;
        _frameEpoch++;
        if (frameEpoch) {
          *frameEpoch = _frameEpoch;
        }
        return texture;
      }
      // No slot available (pool not allocated yet, unsupported device, view
      // detached mid-frame, or a free-slot timeout): fall through to an
      // offscreen frame so the render loop survives; this frame is simply
      // not shown.
    }
#endif
    if (presentation() == Presentation::kDirect) {
      return getCurrentTextureDirect(frameEpoch);
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      throw std::runtime_error(
          "[WebGPU] getCurrentTexture() called on a canvas context that is "
          "not configured; call context.configure() first");
    }
    // Per the WebGPU spec, getCurrentTexture returns the same texture until
    // the frame is presented.
    int index = indexOf(FrameState::kRendering);
    if (index < 0) {
      index = takeFrameLocked();
      _frames[index].state = FrameState::kRendering;
      _frameInFlight = true;
      _frameEpoch++;
    }
    if (frameEpoch) {
      *frameEpoch = _frameEpoch;
    }
    return _frames[index].texture;
  }

  // True while the frame started by the getCurrentTexture() call that
  // returned frameEpoch is still the current one: nothing presented,
  // configured, resized or unconfigured since.
  bool isCurrentFrame(uint64_t frameEpoch) {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _frameInFlight && _frameEpoch == frameEpoch;
  }

  // The frame is done. A direct canvas presents the surface's texture (see
  // presentDirect). A composited one makes the frame the one ready for the
  // window from now, in place of a ready frame the window had not taken yet,
  // and asks the present thread for a frame (see scheduleFrame). Returns at
  // once either way.
  void present() {
#if defined(__ANDROID__)
    if (_poolMode.load()) {
      // Ends access on the pool slot and hands it to the fence waiter; the
      // view picks it up once its render-complete fence signals.
      poolPresent();
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _frameInFlight = false;
      _frameEpoch++;
      return;
    }
#endif
    if (presentation() == Presentation::kDirect) {
      presentDirect();
      return;
    }
    bool ready = false;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      const int rendering = indexOf(FrameState::kRendering);
      if (rendering >= 0) {
        const int previous = indexOf(FrameState::kReady);
        if (previous >= 0) {
          _frames[previous].state = FrameState::kFree;
        }
        _frames[rendering].state = FrameState::kReady;
        ready = true;
      }
      _frameInFlight = false;
      _frameEpoch++;
    }
    if (!ready) {
      return;
    }
    // On the present thread, where the scheduler lives: the canvas may be
    // gone by the time the task runs, and nothing is owed then.
    if (auto context = PlatformContext::current()) {
      std::weak_ptr<SurfaceInfo> weakThis = weak_from_this();
      context->runOnPresentThread([weakThis]() {
        if (auto self = weakThis.lock()) {
          self->scheduleFrame();
        }
      });
    }
  }

  // --- Any thread -----------------------------------------------------------

  NativeInfo getNativeInfo() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return {
        .nativeSurface = _nativeSurface, .width = _width, .height = _height};
  }

  Size getSize() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return {.width = _width, .height = _height};
  }

  wgpu::SurfaceConfiguration getConfig() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _config;
  }

  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // Called right before `device` is destroyed. If the Dawn surface holds a
  // swapchain for that device, drop the surface now, while the device is
  // still alive, and recreate an unconfigured one for the same native window
  // so a later configure() with another device still renders on screen.
  //
  // Why: on Vulkan, tearing a swapchain down (SwapChain::DetachFromSurfaceImpl)
  // goes through the device's FencedDeleter. Dawn only tears it down when the
  // surface is reconfigured or destroyed, and Unconfigure() is not enough (it
  // parks the swapchain as "recycled" for the next Configure). So when the
  // native view is dropped after device.destroy(), ~Surface dereferences the
  // dead device (SIGSEGV, fault addr 0x20). Android only: Metal's detach does
  // not touch the device, and this must not change behaviour there.
  //
  // Waits for the present in flight, if any, on the present thread, which
  // holds the surface for its duration: the one wait on that thread the
  // library makes, as the device must not go under the swapchain.
  //
  // Remove once the pinned Dawn contains the upstream fix; the check in
  // scripts/check-dawn.ts fails when the Dawn the react-native-webgpu-dawn
  // package was built from changes, so that decision is not forgotten.
  void releaseSurfaceForDevice(const wgpu::Device &device) {
#if defined(__ANDROID__)
    std::lock_guard<std::mutex> surfaceLock(_surfaceMutex);
    {
      // The pool's SharedTextureMemory imports belong to the device too: drop
      // them while it is alive. The next getCurrentTexture() after a configure
      // with a live device reallocates the pool.
      std::lock_guard<std::mutex> poolLock(_poolMutex);
      if (_poolDevice && _poolDevice.Get() == device.Get()) {
        poolResetLocked();
        _poolDevice = nullptr;
      }
    }
    if (!_surface || !_surfaceDevice || _surfaceDevice.Get() != device.Get()) {
      return;
    }
    std::unique_lock<std::shared_mutex> lock(_mutex);
    // Left unconfigured: _config still names the destroyed device, and
    // configuring against it would only raise a validation error. The next
    // configure() from JS (or a surface re-attach) configures it.
    recreateSurfaceLocked(_nativeSurface);
    _directAcquired = false;
    _needsConfigure = true;
    _frameEpoch++;
    // A present posted for the device, not run yet, has nothing to present
    // onto any more (see presentPendingFrame).
    _presentPending = false;
    lock.unlock();
    _presentDone.notify_all();
#else
    (void)device;
#endif
  }

  // True while a native view owns a window for this context (attached, or
  // posted to be). Used to decide which side retires the registry entry:
  // the native view's teardown when a view attached, the JS Canvas cleanup
  // otherwise (see RNWebGPU::destroyContext).
  bool hasNativeSurface() {
#if defined(__ANDROID__)
    // A WebGPUNativeBufferView in pool mode owns presentation the same way a
    // surface-backed view does, and retires the entry on its own teardown.
    if (_poolMode.load()) {
      return true;
    }
#endif
    if (_viewAttached.load(std::memory_order_acquire)) {
      return true;
    }
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _nativeSurface != nullptr;
  }

#if defined(__ANDROID__)
  // --- AHB-pool API (called from the JNI layer, see cpp-adapter.cpp) ---------

  // Turn on pool mode for this context. dpW/dpH is the canvas-client (dp) size
  // reported to JS via getSize(); the actual buffers are sized in pool px from
  // the canvas drawing buffer in poolResize().
  void enablePool(int dpW, int dpH) {
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _width = dpW;
      _height = dpH;
    }
    std::lock_guard<std::mutex> poolLock(_poolMutex);
    _poolMode.store(true);
    _shutdown = false;
  }

  // Set the canvas-client (dp) size without changing pool mode. Keeps getSize()
  // (and thus the JS canvas.clientWidth/Height) in sync on resize.
  void setPoolClientSize(int dpW, int dpH) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _width = dpW;
    _height = dpH;
  }

  // (Re)allocate the pool to match the canvas drawing buffer (px). Called from
  // getCurrentTexture() on the rendering thread before acquiring a slot, so the
  // canvas texture size always tracks the app's other attachments.
  void poolResize(int pxW, int pxH) {
    std::lock_guard<std::mutex> poolLock(_poolMutex);
    if (!_poolMode.load() || _poolDevice == nullptr || pxW <= 0 || pxH <= 0) {
      return;
    }
    if (_pool != nullptr && _pool->width == pxW && _pool->height == pxH) {
      return;
    }
    if (_poolUnsupported) {
      return;
    }
    if (!_poolDevice.HasFeature(
            wgpu::FeatureName::SharedTextureMemoryAHardwareBuffer)) {
      _poolUnsupported = true;
      RNWGPU_POOL_WARN("device lacks SharedTextureMemoryAHardwareBuffer; the "
                       "transparent canvas cannot present and stays blank");
      return;
    }
    auto pool = allocatePoolLocked(pxW, pxH);
    if (pool == nullptr) {
      return;
    }
    _pool = pool;
    _pools[pool->generation] = pool;
    // Keep current + previous generation (in-flight frames of the previous size
    // may still be on screen during the cross-fade); drop anything older.
    for (auto it = _pools.begin(); it != _pools.end();) {
      if (it->first + 1 < pool->generation) {
        it = _pools.erase(it);
      } else {
        ++it;
      }
    }
    _freeCv.notify_all();
  }

  // Latest signaled frame awaiting display, encoded as (generation << 32 |
  // slot). Returns -1 when there is nothing new. Marks the slot Displayed so
  // the producer will not re-render into it until releaseSlot() is called.
  // Called on the UI thread after the frame-ready wake-up.
  int64_t poolPollReady() {
    std::lock_guard<std::mutex> poolLock(_poolMutex);
    if (_readySlot < 0 || _readyPool == nullptr) {
      return -1;
    }
    int idx = _readySlot;
    uint32_t gen = _readyPool->generation;
    _readyPool->slots[idx].state = SlotState::Displayed;
    _readyPool = nullptr;
    _readySlot = -1;
    return (static_cast<int64_t>(gen) << 32) | static_cast<uint32_t>(idx);
  }

  // The AHardwareBuffer backing a (generation, slot), for the consumer to wrap
  // in a Bitmap. Returns nullptr for a retired generation. Called on the UI
  // thread; the JNI layer converts it to a HardwareBuffer jobject.
  void *poolBufferForDisplay(uint32_t gen, int slot) {
    std::lock_guard<std::mutex> poolLock(_poolMutex);
    auto it = _pools.find(gen);
    if (it == _pools.end() || slot < 0 ||
        slot >= static_cast<int>(it->second->slots.size())) {
      return nullptr;
    }
    return it->second->slots[slot].ahb;
  }

  // The consumer is done displaying (and holding) a slot: return it to the free
  // list so the producer may render into it again. No-op for a retired
  // generation (those buffers are never reused). Called on the UI thread.
  void poolReleaseSlot(uint32_t gen, int idx) {
    std::lock_guard<std::mutex> poolLock(_poolMutex);
    if (_pool != nullptr && _pool->generation == gen && idx >= 0 &&
        idx < static_cast<int>(_pool->slots.size())) {
      _pool->slots[idx].state = SlotState::Free;
      _freeCv.notify_one();
    }
  }

  // Invoked from the waiter thread (never under _poolMutex) each time a new
  // frame becomes ready, so the consumer can wake up instead of polling every
  // vsync. Pass nullptr to unregister.
  void setPoolFrameReadyCallback(std::function<void()> cb) {
    std::lock_guard<std::mutex> poolLock(_poolMutex);
    _poolFrameReady = std::move(cb);
  }

  // Leaving pool mode (the WebGPUHardwareBufferView detached): drop the pool.
  // A configured context keeps rendering, into its frames (see the class
  // comment), exactly like a canvas whose window went. Main thread.
  void leavePool() {
    // Destroyed outside the lock: it releases a JNI reference.
    std::function<void()> releasedFrameReady;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (!_poolMode.load()) {
        return;
      }
      _poolMode.store(false);
      {
        std::lock_guard<std::mutex> poolLock(_poolMutex);
        // A frame between getCurrentTexture and present still has an open
        // BeginAccess; end it and queue it so the waiter drains its fences
        // before the generation is freed (present() no-ops for it because
        // pool mode is already off).
        if (_renderPool != nullptr && _currentRenderSlot >= 0 &&
            _renderPool->slots[_currentRenderSlot].state ==
                SlotState::Rendering) {
          poolQueuePresentLocked();
        }
        // _presentQueue is intentionally left alone: the waiter still has
        // to wait on the queued fences before the generations they pin can
        // be released; it discards them on publish (no live pool).
        poolResetLocked();
        releasedFrameReady = std::move(_poolFrameReady);
        _poolFrameReady = nullptr;
      }
      _frameInFlight = false;
      _frameEpoch++;
    }
  }
#endif

private:
  // What the scheduler asks of the canvas (see rnsurface::FrameScheduler): the
  // vsync from the platform view, the present from here.
  class Host : public rnsurface::FrameScheduler::Host {
  public:
    explicit Host(SurfaceInfo &info) : _info(info) {}
    void postFrameCallback() override {
      if (_info._postFrameCallback) {
        _info._postFrameCallback();
      }
    }
    int presentFrame() override { return _info.presentFrame(); }

  private:
    SurfaceInfo &_info;
  };

  // See the class comment.
  enum class FrameState {
    kFree,
    kRendering,
    kReady,
    kPresenting,
    kShown,
  };

  struct Frame {
    wgpu::Texture texture = nullptr;
    FrameState state = FrameState::kFree;
  };

  // What detach() does, without the thread check: the destructor runs
  // wherever the last owner lets go of the canvas.
  void detachNow() {
    std::lock_guard<std::mutex> surfaceLock(_surfaceMutex);
    releaseWindowLocked();
    // The window the platform took back is let go of now (see abandon). Only
    // here, not in attach(): an abandon is always followed by this detach,
    // and an attach for another window comes behind it.
    _abandoned.store(false, std::memory_order_release);
    _scheduler.setWindow(Window::kNone);
  }

  // Lets go of the Dawn surface and gives the window back to its releaser,
  // leaving the abandon, if any, up: what detach() does before it takes the
  // abandon down, and what attach() does to the previous window. Caller holds
  // _surfaceMutex.
  void releaseWindowLocked() {
    // The surface goes before the window it draws into.
    _surface = nullptr;
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    _surfaceDevice = nullptr;
#endif
    void *nativeSurface = nullptr;
    NativeSurfaceReleaser releaser;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      nativeSurface = _nativeSurface;
      releaser = std::move(_releaser);
      _nativeSurface = nullptr;
      _releaser = nullptr;
      _needsConfigure = false;
      _directAcquired = false;
      // A present posted for the window, not run yet, has nothing to present
      // onto any more (see presentPendingFrame): the render runtime waiting
      // for it draws on.
      _presentPending = false;
    }
    _presentDone.notify_all();
    // Outside _mutex: the platform may do real work here.
    if (releaser && nativeSurface != nullptr) {
      releaser(nativeSurface);
    }
  }

  // The index of the frame in `state`, or -1. Caller holds _mutex.
  int indexOf(FrameState state) const {
    for (size_t i = 0; i < _frames.size(); i++) {
      if (_frames[i].texture != nullptr && _frames[i].state == state) {
        return static_cast<int>(i);
      }
    }
    return -1;
  }

  // Lets go of every frame. Caller holds _mutex.
  void dropFramesLocked() {
    for (auto &frame : _frames) {
      frame = Frame();
    }
    _fallback = nullptr;
    _current = nullptr;
    _showLastFrame = false;
    _frameInFlight = false;
    _directAcquired = false;
    _frameEpoch++;
  }

  // Whether a frame can go onto the drawing buffer as configured. Caller
  // holds _mutex.
  bool matchesConfigLocked(const wgpu::Texture &texture) const {
    return texture != nullptr && texture.GetWidth() == _config.width &&
           texture.GetHeight() == _config.height &&
           texture.GetFormat() == _config.format &&
           texture.GetUsage() == frameUsageLocked();
  }

  // The usage of an offscreen texture: for a direct canvas, what the canvas
  // was configured with, as the window's texture it stands in for has; for a
  // composited canvas's frame, that, what rendering into it needs, and the
  // copy onto the window (CopySrc, which the texture reports beyond what the
  // canvas was configured with).
  wgpu::TextureUsage frameUsageLocked() const {
    if (presentation() == Presentation::kDirect) {
      return _config.usage;
    }
    return _config.usage | wgpu::TextureUsage::RenderAttachment |
           wgpu::TextureUsage::CopySrc;
  }

  // The frame for the render runtime to draw into: a free one, a new one
  // while there is room, else the one the window shows (its copy is
  // submitted, see the class comment). Frames made for another drawing
  // buffer are let go of on the way. Caller holds _mutex.
  int takeFrameLocked() {
    int shown = -1;
    for (size_t i = 0; i < _frames.size(); i++) {
      Frame &frame = _frames[i];
      if (frame.texture == nullptr) {
        continue;
      }
      if (frame.state != FrameState::kPresenting &&
          !matchesConfigLocked(frame.texture)) {
        frame = Frame();
        continue;
      }
      if (frame.state == FrameState::kShown) {
        shown = static_cast<int>(i);
      }
    }
    const int free = indexOf(FrameState::kFree);
    if (free >= 0) {
      return free;
    }
    for (size_t i = 0; i < _frames.size(); i++) {
      if (_frames[i].texture == nullptr) {
        _frames[i].texture = createFrameTextureLocked();
        _frames[i].state = FrameState::kFree;
        return static_cast<int>(i);
      }
    }
    if (shown >= 0) {
      _showLastFrame = false;
      return shown;
    }
    // Every frame is ready or being presented, which the states rule out:
    // the ready one is the render runtime's to draw over.
    const int ready = indexOf(FrameState::kReady);
    return ready >= 0 ? ready : 0;
  }

  wgpu::Texture createFrameTextureLocked() {
    wgpu::TextureDescriptor descriptor;
    descriptor.usage = frameUsageLocked();
    descriptor.format = _config.format;
    descriptor.size.width = std::max(1u, _config.width);
    descriptor.size.height = std::max(1u, _config.height);
    descriptor.viewFormats = _config.viewFormats;
    descriptor.viewFormatCount = _config.viewFormatCount;
    return _config.device.CreateTexture(&descriptor);
  }

  // What configuring the window takes, copied out from under _mutex so that
  // the surface's own calls hold _surfaceMutex alone: the swapchain's
  // configuration, and on Apple platforms the layer to tag with the canvas's
  // color space.
  struct WindowConfiguration {
    wgpu::SurfaceConfiguration config;
    void *nativeSurface = nullptr;
    CanvasColorSpace colorSpace = CanvasColorSpace::SRGB;
    CanvasToneMappingMode toneMappingMode = CanvasToneMappingMode::Standard;
  };

  // The configuration of the window's swapchain: the drawing buffer's format
  // and size, and the usage the copy of a frame onto it needs. The render
  // runtime's usage and view formats apply to the frames, not to the window;
  // in direct mode the window's texture is the frame, and gets them. Caller
  // holds _mutex.
  WindowConfiguration windowConfigurationLocked(bool direct) const {
    WindowConfiguration window;
    window.nativeSurface = _nativeSurface;
    window.colorSpace = _colorSpace;
    window.toneMappingMode = _toneMappingMode;
    if (direct) {
      window.config = _config;
      return window;
    }
    window.config.device = _config.device;
    window.config.format = _config.format;
    window.config.width = _config.width;
    window.config.height = _config.height;
    window.config.presentMode = wgpu::PresentMode::Fifo;
    window.config.usage =
        wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst;
#ifdef __APPLE__
    window.config.alphaMode = _config.alphaMode;
#endif
    return window;
  }

  // Copies a frame onto the window's texture, on the device's queue: before
  // anything the render runtime submits into the frame afterwards.
  static void copyTexture(const wgpu::Device &device,
                          const wgpu::Texture &source,
                          const wgpu::Texture &destination) {
    wgpu::TexelCopyTextureInfo from = {};
    from.texture = source;
    wgpu::TexelCopyTextureInfo to = {};
    to.texture = destination;
    // The two agree on size, but for a window configured again meanwhile:
    // copy the shared region.
    wgpu::Extent3D size = {
        std::min(source.GetWidth(), destination.GetWidth()),
        std::min(source.GetHeight(), destination.GetHeight()), 1};
    wgpu::CommandEncoderDescriptor encoderDescriptor;
    wgpu::CommandEncoder encoder =
        device.CreateCommandEncoder(&encoderDescriptor);
    encoder.CopyTextureToTexture(&from, &to, &size);
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);
  }

  // Acquire the surface's current texture, reconfiguring once when the surface
  // reports it is stale (rotation, resize, coming back from background).
  // Caller holds _surfaceMutex.
  wgpu::Texture acquireSurfaceTextureLocked(const WindowConfiguration &config) {
    wgpu::SurfaceTexture surfaceTexture;
    _surface.GetCurrentTexture(&surfaceTexture);
    if (!isAcquireSuccess(surfaceTexture)) {
      if (surfaceTexture.status ==
          wgpu::SurfaceGetCurrentTextureStatus::Error) {
        return nullptr;
      }
      configureSurfaceLocked(config);
      // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE: the reconfigure
      // may have replaced the surface and failed to create a new one.
      if (!_surface) {
        return nullptr;
      }
      _surface.GetCurrentTexture(&surfaceTexture);
      if (!isAcquireSuccess(surfaceTexture)) {
        return nullptr;
      }
    }
    return surfaceTexture.texture;
  }

  static bool isAcquireSuccess(const wgpu::SurfaceTexture &surfaceTexture) {
    return (surfaceTexture.status ==
                wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
            surfaceTexture.status ==
                wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) &&
           surfaceTexture.texture != nullptr;
  }

  // Every Surface::Configure goes through here, and tags the layer with the
  // canvas's color space on Apple platforms. Caller holds _surfaceMutex, and
  // not _mutex, which the configuration was copied out from under. Once the
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE lines are gone, this
  // is a plain wrapper around _surface.Configure.
  void configureSurfaceLocked(const WindowConfiguration &window) {
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    // Vulkan refuses to switch a surface to another device and keeps the old
    // swapchain attached, so a device change (device swap, recovery after a
    // lost device, a re-run effect) is done with a fresh surface. This also
    // tears the old swapchain down while its device is still alive.
    if (_surfaceDevice && _surfaceDevice.Get() != window.config.device.Get()) {
      recreateSurfaceLocked(window.nativeSurface);
      if (!_surface) {
        return;
      }
    }
#endif
    _surface.Configure(&window.config);
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    _surfaceDevice = window.config.device;
#endif
#ifdef __APPLE__
    if (window.nativeSurface != nullptr) {
      applyCAMetalLayerColorSpace(window.nativeSurface, window.config.format,
                                  window.colorSpace, window.toneMappingMode);
    }
#endif
  }

  // Replace _surface with a new, unconfigured surface for the same native
  // window: after unconfigure() destroyed it (see the constructor), or on
  // Android for DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE, where
  // dropping the old one runs ~Surface, which detaches both the active and
  // the recycled swapchain against their still-alive device. Caller holds
  // _surfaceMutex, and configures the new surface, or marks it to be (a frame
  // acquired from the old one can no longer be presented).
  void recreateSurfaceLocked(void *nativeSurface) {
    _surface = nullptr;
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    _surfaceDevice = nullptr;
#endif
    if (nativeSurface != nullptr) {
      _surface = WebGPURuntime::makeSurface(_gpu, nativeSurface);
    }
  }

  // --- Direct presentation (see the class comment) -------------------------

  // The present posted by presentDirect() is not done yet: the window has
  // not handed out its next texture, and the surface is the present
  // thread's until then. Render runtime; takes and releases _mutex.
  void waitForPendingPresent() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _presentDone.wait(lock, [this] { return !_presentPending; });
  }

  // Acquires the surface's texture for the frame, once the present in flight
  // is done with the window (see the class comment), configuring the surface
  // when the window or the drawing buffer changed since. Without a window,
  // while the app cannot show a frame (see setCanPresent), once the platform
  // took the window back (see abandon) or when the window hands out no
  // texture (mid-resize, lost while the app was in the background), the frame
  // is rendered into an offscreen texture instead, so that the render loop
  // goes on; that frame is not presented. Render runtime.
  wgpu::Texture getCurrentTextureDirect(uint64_t *frameEpoch) {
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_config.device == nullptr) {
        throw std::runtime_error(
            "[WebGPU] getCurrentTexture() called on a canvas context that is "
            "not configured; call context.configure() first");
      }
      if (_frameInFlight && _current != nullptr) {
        if (frameEpoch) {
          *frameEpoch = _frameEpoch;
        }
        return _current;
      }
      // The frame presented last is on its way to the window (see
      // presentDirect): the window hands out its next texture once it is
      // there. Outside the surface's lock, which the present holds.
      _presentDone.wait(lock, [this] { return !_presentPending; });
    }
    std::lock_guard<std::mutex> surfaceLock(_surfaceMutex);
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      throw std::runtime_error(
          "[WebGPU] getCurrentTexture() called on a canvas context that is "
          "not configured; call context.configure() first");
    }
    _frameInFlight = true;
    _directAcquired = false;
    _frameEpoch++;
    if (frameEpoch) {
      *frameEpoch = _frameEpoch;
    }
    if (_surface && !_abandoned.load(std::memory_order_acquire) &&
        (!_canPresent || _canPresent())) {
      const WindowConfiguration window =
          windowConfigurationLocked(/* direct = */ true);
      if (_needsConfigure) {
        _needsConfigure = false;
        configureSurfaceLocked(window);
      }
      auto texture = _surface ? acquireSurfaceTextureLocked(window) : nullptr;
      if (texture) {
        _directAcquired = true;
        _current = texture;
        return texture;
      }
      // The surface is transiently unusable (e.g. mid-resize, lost while
      // backgrounding): fall back to an offscreen texture so the render loop
      // survives; this frame is simply not presented.
    }
    if (!_fallback || !matchesConfigLocked(_fallback)) {
      _fallback = createFrameTextureLocked();
    }
    _current = _fallback;
    return _current;
  }

  // Direct presentation's present(): the surface's texture acquired for the
  // frame goes to the window. On the present thread when the device
  // synchronizes itself (see configure), behind the presents of the other
  // canvases, so that the render runtime does not pay for it: the next
  // getCurrentTexture() waits for it instead (see getCurrentTextureDirect).
  // On this thread otherwise, or when the present thread is out of reach. A
  // frame rendered into the offscreen texture (see getCurrentTextureDirect)
  // is dropped. Render runtime.
  void presentDirect() {
    auto context = PlatformContext::current();
    wgpu::Device device;
    bool present;
    bool offThread;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      present = _directAcquired;
      device = _config.device;
      offThread = present && _deviceSynchronized && context != nullptr;
      _directAcquired = false;
      _frameInFlight = false;
      _current = nullptr;
      _frameEpoch++;
      if (offThread) {
        _presentPending = true;
      }
    }
    if (!present) {
      return;
    }
    if (offThread) {
      // The canvas may be gone by the time the task runs, and nothing is
      // owed then.
      std::weak_ptr<SurfaceInfo> weakThis = weak_from_this();
      const bool posted = context->runOnPresentThread([weakThis, device]() {
        if (auto self = weakThis.lock()) {
          self->presentPendingFrame(device);
        }
      });
      if (posted) {
        return;
      }
    }
    // Here, then: the frame must not leave the next acquire waiting for a
    // present that never comes.
    std::function<void()> framePresented;
    {
      std::lock_guard<std::mutex> surfaceLock(_surfaceMutex);
      const bool presented = presentSurfaceLocked(device);
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _presentPending = false;
      if (presented) {
        framePresented = _framePresented;
      }
    }
    if (offThread) {
      _presentDone.notify_all();
    }
    if (framePresented) {
      framePresented();
    }
  }

  // The present presentDirect() posted: presents the surface's texture, if
  // the window is still there to take it, and lets the render runtime acquire
  // the next one either way. A window detached meanwhile took the frame with
  // it (see releaseWindowLocked). Present thread.
  void presentPendingFrame(const wgpu::Device &device) {
    RNSURFACE_ASSERT_THREAD(_presentThread, "SurfaceInfo::presentPendingFrame");
    std::function<void()> framePresented;
    {
      std::lock_guard<std::mutex> surfaceLock(_surfaceMutex);
      bool pending;
      {
        std::shared_lock<std::shared_mutex> lock(_mutex);
        pending = _presentPending;
      }
      const bool presented = pending && presentSurfaceLocked(device);
      std::unique_lock<std::shared_mutex> lock(_mutex);
      _presentPending = false;
      if (presented) {
        framePresented = _framePresented;
      }
    }
    _presentDone.notify_all();
    if (framePresented) {
      framePresented();
    }
  }

  // Presents the surface's texture, unless the window is gone, or going (see
  // abandon), or cannot show a frame (see setCanPresent): the texture stays
  // the window's current one then, and the next frame draws over it. Caller
  // holds _surfaceMutex. Says whether a frame went to the window.
  bool presentSurfaceLocked(const wgpu::Device &device) {
    std::function<bool()> canPresent;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      canPresent = _canPresent;
    }
    if (_surface == nullptr || _abandoned.load(std::memory_order_acquire) ||
        (canPresent && !canPresent())) {
      return false;
    }
#ifdef __APPLE__
    // Ensure command buffers are scheduled before presenting.
    if (device) {
      dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
    }
#endif
    _surface.Present();
    return true;
  }

#if defined(__ANDROID__)
  // Drop every pool generation and the ready / in-flight slot bookkeeping.
  // Caller holds _poolMutex. Generations pinned by a queued present survive
  // through their PendingPresent until the waiter is done with them.
  void poolResetLocked() {
    _pool = nullptr;
    _pools.clear();
    _readyPool = nullptr;
    _readySlot = -1;
    _renderPool = nullptr;
    _currentRenderSlot = -1;
    _freeCv.notify_all();
  }

  // Allocate + import one generation of the pool. Caller holds _poolMutex.
  std::shared_ptr<AHBPool> allocatePoolLocked(int w, int h) {
    auto pool = std::make_shared<AHBPool>();
    pool->generation = ++_genCounter;
    pool->width = w;
    pool->height = h;
    pool->slots.resize(kPoolSize);
    for (auto &slot : pool->slots) {
      AHardwareBuffer_Desc desc = {};
      desc.width = static_cast<uint32_t>(w);
      desc.height = static_cast<uint32_t>(h);
      desc.layers = 1;
      desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
      desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
      int err = AHardwareBuffer_allocate(&desc, &slot.ahb);
      if (err != 0 || slot.ahb == nullptr) {
        RNWGPU_POOL_WARN("AHardwareBuffer_allocate failed (%d) %dx%d", err, w,
                         h);
        return nullptr;
      }
      wgpu::SharedTextureMemoryDescriptor memDesc{};
      wgpu::SharedTextureMemoryAHardwareBufferDescriptor ahbDesc{};
      ahbDesc.handle = slot.ahb;
      memDesc.nextInChain = &ahbDesc;
      slot.memory = _poolDevice.ImportSharedTextureMemory(&memDesc);
      // A failed import yields a non-null Dawn error object, so probing the
      // properties is the actual validity check.
      wgpu::SharedTextureMemoryProperties props{};
      if (slot.memory == nullptr ||
          slot.memory.GetProperties(&props) != wgpu::Status::Success) {
        RNWGPU_POOL_WARN("ImportSharedTextureMemory failed (%dx%d)", w, h);
        return nullptr;
      }
      // The AHB is rgba8unorm; the pool cannot honor another configured format.
      // The canvas is configured rgba8unorm on Android (the preferred format),
      // so this only fires for apps hardcoding e.g. bgra8unorm.
      if (_poolFormat != wgpu::TextureFormat::Undefined &&
          _poolFormat != wgpu::TextureFormat::RGBA8Unorm) {
        RNWGPU_POOL_WARN("configured canvas format %d is not supported by the "
                         "transparent canvas; using rgba8unorm (use "
                         "navigator.gpu.getPreferredCanvasFormat())",
                         static_cast<int>(_poolFormat));
      }
      // Honor the configured usage flags as far as the imported memory allows.
      wgpu::TextureUsage wanted =
          _poolUsage | wgpu::TextureUsage::RenderAttachment;
      wgpu::TextureUsage usage = wanted & props.usage;
      if (usage != wanted) {
        RNWGPU_POOL_WARN("configured canvas usage 0x%llx narrowed to 0x%llx "
                         "(unsupported by the transparent canvas buffers)",
                         static_cast<unsigned long long>(wanted),
                         static_cast<unsigned long long>(usage));
      }
      wgpu::TextureDescriptor texDesc{};
      texDesc.format = props.format;
      texDesc.usage = usage;
      texDesc.size.width = static_cast<uint32_t>(w);
      texDesc.size.height = static_cast<uint32_t>(h);
      slot.texture = slot.memory.CreateTexture(&texDesc);
      slot.state = SlotState::Free;
    }
    return pool;
  }

  wgpu::Texture poolGetCurrentTexture() {
    std::unique_lock<std::mutex> lock(_poolMutex);
    // Per the WebGPU spec, getCurrentTexture returns the same texture until the
    // frame is presented; a repeat call must not consume another slot.
    if (_renderPool != nullptr && _currentRenderSlot >= 0) {
      return _renderPool->slots[_currentRenderSlot].texture;
    }
    if (_pool == nullptr) {
      return nullptr; // allocation failed / unsupported (already reported)
    }
    auto pool = _pool;
    int idx = -1;
    // Bounded wait: if no slot frees up (e.g. a wedged fence), skip the frame
    // instead of parking the JS/worklet thread forever.
    bool ready = _freeCv.wait_for(lock, std::chrono::seconds(1), [&]() {
      if (_shutdown || pool != _pool) {
        return true; // pool replaced or shutting down: bail out
      }
      for (size_t i = 0; i < pool->slots.size(); i++) {
        if (pool->slots[i].state == SlotState::Free) {
          idx = static_cast<int>(i);
          return true;
        }
      }
      return false;
    });
    if (!ready) {
      RNWGPU_POOL_WARN("poolGetCurrentTexture: timed out waiting for a free "
                       "slot; skipping frame");
      return nullptr;
    }
    if (idx < 0 || _shutdown || pool != _pool) {
      return nullptr;
    }
    auto &slot = pool->slots[idx];
    slot.state = SlotState::Rendering;
    _renderPool = pool;
    _currentRenderSlot = idx;
    poolBeginAccess(slot);
    return slot.texture;
  }

  void poolPresent() {
    std::lock_guard<std::mutex> lock(_poolMutex);
    if (_currentRenderSlot < 0 || _renderPool == nullptr) {
      return;
    }
    poolQueuePresentLocked();
  }

  // Ends access on the in-flight render slot and hands it (with its fences) to
  // the waiter. Caller holds _poolMutex and guarantees _renderPool /
  // _currentRenderSlot are valid.
  void poolQueuePresentLocked() {
    auto pool = _renderPool;
    int idx = _currentRenderSlot;
    auto &slot = pool->slots[idx];

    wgpu::SharedTextureMemoryEndAccessState state{};
    wgpu::SharedTextureMemoryVkImageLayoutEndState vkLayout{};
    state.nextInChain = &vkLayout;
    slot.memory.EndAccess(slot.texture, &state);

    std::vector<wgpu::SharedFence> fences;
    fences.reserve(state.fenceCount);
    for (size_t i = 0; i < state.fenceCount; i++) {
      fences.push_back(state.fences[i]);
    }

    slot.state = SlotState::Presented;
    _presentQueue.push_back({pool, idx, std::move(fences)});
    _renderPool = nullptr;
    _currentRenderSlot = -1;
    startWaiterLocked();
    _presentCv.notify_one();
  }

  void poolBeginAccess(PoolSlot &slot) {
    wgpu::SharedTextureMemoryBeginAccessDescriptor desc{};
    desc.initialized = false; // canvas contents are fully redrawn each frame
    desc.concurrentRead = false;
    desc.fenceCount = 0;
    desc.fences = nullptr;
    desc.signaledValues = nullptr;
    wgpu::SharedTextureMemoryVkImageLayoutBeginState vkLayout{};
    vkLayout.oldLayout = 0;
    vkLayout.newLayout = 0;
    desc.nextInChain = &vkLayout;
    slot.memory.BeginAccess(slot.texture, &desc);
  }

  void startWaiterLocked() {
    if (_waiterRunning) {
      return;
    }
    _waiterRunning = true;
    _waiter = std::thread([this]() { waiterLoop(); });
  }

  void stopWaiter() {
    {
      std::lock_guard<std::mutex> lock(_poolMutex);
      _shutdown = true;
      _presentCv.notify_all();
      _freeCv.notify_all();
    }
    if (_waiter.joinable()) {
      _waiter.join();
    }
    _waiterRunning = false;
  }

  // Blocks on each presented frame's render-complete fence (off the UI and JS
  // threads), then publishes it as the latest ready slot. This is the rigorous
  // acquire-side wait: HWUI only samples a buffer after the UI thread picks it
  // up via poolPollReady, which happens strictly after this wait returns.
  void waiterLoop() {
    while (true) {
      PendingPresent pending;
      {
        std::unique_lock<std::mutex> lock(_poolMutex);
        _presentCv.wait(lock,
                        [&]() { return _shutdown || !_presentQueue.empty(); });
        if (_shutdown && _presentQueue.empty()) {
          return;
        }
        pending = std::move(_presentQueue.front());
        _presentQueue.pop_front();
      }

      // Wait outside the lock. Each fd is owned by its SharedFence and closed
      // when `pending.fences` is destroyed at the end of this iteration, so we
      // never dup / double-close (avoids the fdsan abort, see
      // GPUSharedFence.cpp).
      for (auto &fence : pending.fences) {
        wgpu::SharedFenceExportInfo info{};
        wgpu::SharedFenceSyncFDExportInfo fdInfo{};
        info.nextInChain = &fdInfo;
        fence.ExportInfo(&info);
        if (info.type != wgpu::SharedFenceType::SyncFD) {
          // fdInfo was not populated (its default handle 0 is NOT a fence fd);
          // we have no way to wait on this fence type.
          RNWGPU_POOL_WARN("waiter: unexpected shared fence type %d, cannot "
                           "wait for render completion",
                           static_cast<int>(info.type));
          continue;
        }
        if (fdInfo.handle >= 0) {
          // Sync-fence fds become readable (POLLIN) when signaled; poll()
          // avoids any libsync linkage concern. Poll in bounded slices so
          // teardown (which flips _shutdown) cannot hang here on a wedged GPU.
          struct pollfd pfd;
          pfd.fd = fdInfo.handle;
          pfd.events = POLLIN;
          while (!_shutdown.load()) {
            pfd.revents = 0;
            int r = poll(&pfd, 1, 100);
            if (r > 0) {
              break; // signaled (or POLLERR): stop waiting
            }
            if (r < 0 && errno != EINTR) {
              break; // real poll error; EINTR just retries
            }
          }
        }
      }

      std::function<void()> frameReady;
      {
        std::lock_guard<std::mutex> lock(_poolMutex);
        // Pool torn down (leavePool) while this frame was in flight:
        // discard it now that its fences have been drained. Publishing it would
        // pin the retired generation forever (nothing polls anymore).
        if (_pool == nullptr) {
          continue;
        }
        // Supersede a still-unclaimed ready frame: drop it back to Free if it
        // belongs to the live pool (a retired pool is simply discarded).
        if (_readyPool != nullptr && _readySlot >= 0) {
          if (_readyPool == _pool) {
            _readyPool->slots[_readySlot].state = SlotState::Free;
            _freeCv.notify_one();
          }
        }
        pending.pool->slots[pending.slot].state = SlotState::Ready;
        _readyPool = pending.pool;
        _readySlot = pending.slot;
        frameReady = _poolFrameReady;
      }
      if (frameReady) {
        frameReady(); // outside the lock: it calls into Java
      }
    }
  }
#endif

  // The Dawn surface over the window (see the class comment on locks).
  std::mutex _surfaceMutex;
  mutable std::shared_mutex _mutex;
#if defined(__ANDROID__)
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // The device _surface was last configured with. Dawn keeps a swapchain for
  // that device inside the surface even after Unconfigure() (it is recycled
  // for the next Configure), so this outlives _config.device on purpose and
  // is only reset when _surface itself is dropped or replaced. Owning: Dawn
  // destroys a device when its last external reference goes away, which
  // would leave the swapchain with a dead device just like an explicit
  // destroy(). Declared before _surface so it is released after it.
  //
  // Android only: this is an extra owning reference to the device, and on
  // Metal (where the detach never touches the device) it would keep a
  // device alive past its last JS reference for no reason.
  wgpu::Device _surfaceDevice = nullptr;
#endif
  // The window attached (null while detached) and the surface over it.
  // _surfaceMutex guards the surface; _nativeSurface and the releaser are
  // read under _mutex by any thread.
  void *_nativeSurface = nullptr;
  wgpu::Surface _surface = nullptr;
  NativeSurfaceReleaser _releaser;
  // The window attached, or the drawing buffer changed, since the swapchain
  // was configured: the next present configures it.
  bool _needsConfigure = false;
  // The frames of a composited canvas (see the class comment), and whether
  // the one the window shows goes to it again on the next present: a window
  // that came back.
  std::array<Frame, kMaxFrames> _frames;
  bool _showLastFrame = false;
  // Direct presentation: the texture of the frame in flight, acquired from
  // the surface (_directAcquired) or the offscreen fallback, and whether a
  // present posted to the present thread has not run yet (see presentDirect;
  // _presentDone is signalled under _mutex as it does).
  wgpu::Texture _current = nullptr;
  wgpu::Texture _fallback = nullptr;
  bool _directAcquired = false;
  bool _presentPending = false;
  std::condition_variable_any _presentDone;
  // Whether the device configured may be used from the present thread (see
  // synchronizes). Under _mutex.
  bool _deviceSynchronized = false;
  // Frame state: set by getCurrentTexture, cleared by present.
  bool _frameInFlight = false;
  // Bumped on every acquire, present, configure/reconfigure/unconfigure and
  // detach: what isCurrentFrame compares.
  uint64_t _frameEpoch = 0;
  // device == nullptr means "not configured". _viewFormats owns the storage
  // that _config.viewFormats points at.
  wgpu::SurfaceConfiguration _config;
  std::vector<wgpu::TextureFormat> _viewFormats;
  CanvasColorSpace _colorSpace = CanvasColorSpace::SRGB;
  CanvasToneMappingMode _toneMappingMode = CanvasToneMappingMode::Standard;
  // Keeps the Dawn instance alive for as long as any canvas exists.
  wgpu::Instance _gpu;
  // Native view size in dp (surfaced as clientWidth/clientHeight on the JS
  // canvas).
  int _width;
  int _height;
  // See the constructor.
  const bool _ownsWindow;
  // See setPresentation. Fixed from the first configure() on, which is what
  // getCurrentTexture() and present() need it for.
  std::atomic<Presentation> _presentation{Presentation::kDirect};
  // See setViewAttached and abandon.
  std::atomic<bool> _viewAttached{false};
  std::atomic<bool> _abandoned{false};
  // See setFramePresented and setCanPresent; read under _mutex, called
  // outside it.
  std::function<void()> _framePresented;
  std::function<bool()> _canPresent;
  // Present thread only (see the setter).
  std::function<void()> _postFrameCallback;
  // The thread the window belongs to (see the class comment).
  rnsurface::ThreadChecker _presentThread;
  // The scheduler and its host, present thread only. Declared last: the host
  // refers to this object, the scheduler to the host.
  Host _host;
  rnsurface::FrameScheduler _scheduler;
#if defined(__ANDROID__)
  // Pool state. Guarded by _poolMutex (never nested under _mutex while
  // blocking).
  struct PendingPresent {
    std::shared_ptr<AHBPool> pool;
    int slot;
    std::vector<wgpu::SharedFence> fences;
  };

  static constexpr int kPoolSize =
      5; // 2 held + 1 rendering + 1 waiting + 1 ready

  std::atomic<bool> _poolMode{false};
  std::mutex _poolMutex;
  std::condition_variable _freeCv;    // a slot returned to Free
  std::condition_variable _presentCv; // a frame was queued for the waiter
  wgpu::Device _poolDevice = nullptr;
  wgpu::TextureFormat _poolFormat = wgpu::TextureFormat::Undefined;
  wgpu::TextureUsage _poolUsage = wgpu::TextureUsage::RenderAttachment;
  bool _poolUnsupported = false;  // device lacks AHB shared-texture support
  std::shared_ptr<AHBPool> _pool; // current generation
  std::unordered_map<uint32_t, std::shared_ptr<AHBPool>> _pools; // gen -> pool
  std::shared_ptr<AHBPool> _renderPool; // generation of the in-flight render
  int _currentRenderSlot = -1;
  std::shared_ptr<AHBPool> _readyPool; // generation of the latest ready frame
  int _readySlot = -1;
  std::deque<PendingPresent> _presentQueue;
  std::thread _waiter;
  bool _waiterRunning = false;
  std::atomic<bool> _shutdown{false};
  uint32_t _genCounter = 0;
  std::function<void()> _poolFrameReady; // consumer wake-up, see setter
#endif
};

class SurfaceRegistry {
public:
  static SurfaceRegistry &getInstance() {
    static SurfaceRegistry instance;
    return instance;
  }

  SurfaceRegistry(const SurfaceRegistry &) = delete;
  SurfaceRegistry &operator=(const SurfaceRegistry &) = delete;

  // The id of a Canvas about to mount. Ids are handed out here, once for the
  // life of the process, rather than counted by each JS runtime from the
  // same start: this registry outlives a reload, and the views of the bundle
  // being replaced are torn down (removeSurfaceInfo) on the UI thread with
  // no ordering against the new bundle's first canvases. An id counted from
  // the same start as its predecessor would let the predecessor's teardown
  // detach and erase the entry of a live canvas.
  int allocateContextId() { return _nextContextId.fetch_add(1); }

  std::shared_ptr<SurfaceInfo> getSurfaceInfo(int id) {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(id);
    if (it != _registry.end()) {
      return it->second;
    }
    return nullptr;
  }

  void removeSurfaceInfo(int id) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.erase(id);
    _retired.erase(id);
  }

  std::shared_ptr<SurfaceInfo>
  getSurfaceInfoOrCreate(int id, wgpu::Instance gpu, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    return getSurfaceInfoOrCreateLocked(id, gpu, width, height);
  }

  // Find-or-create + the view's claim on the entry as one atomic step under
  // the registry lock, so it serializes with removeSurfaceInfoIfDetached: a
  // view can never land on an entry that a concurrent destroyContext is
  // erasing (it either marks the entry attached before the check, or
  // re-creates the entry after the erase). The main thread calls it as it
  // posts the window's attach to the present thread (see
  // SurfaceInfo::setViewAttached), with how the canvas presents (see
  // SurfaceInfo::setPresentation). Lock order is registry -> SurfaceInfo,
  // matching every other path.
  std::shared_ptr<SurfaceInfo>
  attachView(int id, wgpu::Instance gpu, int width, int height,
             SurfaceInfo::Presentation presentation) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto info = getSurfaceInfoOrCreateLocked(id, gpu, width, height);
    info->setPresentation(presentation);
    info->setViewAttached(true);
    return info;
  }

  // Erase the entry only if no native surface is attached or pending; the
  // atomic counterpart of attachView above (see RNWebGPU::destroyContext
  // for the ownership split this implements). The SurfaceInfo is let go of,
  // not forgotten: for as long as a GPUCanvasContext holds it, the next
  // find-or-create for the id (the native view's attach, or another context)
  // takes it over. React's StrictMode runs the Canvas cleanup once right after
  // mount, before a lagging UI thread attached the surface, and the context
  // the first effect run made stays in use; it would otherwise render into a
  // SurfaceInfo no view ever attaches to.
  void removeSurfaceInfoIfDetached(int id) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(id);
    if (it == _registry.end() || it->second->hasNativeSurface()) {
      return;
    }
    pruneRetiredLocked();
    if (it->second.use_count() > 1) {
      // A GPUCanvasContext holds it: keep it reachable for the next
      // find-or-create (see getSurfaceInfoOrCreateLocked).
      _retired[id] = it->second;
    }
    _registry.erase(it);
  }

  // A SurfaceInfo that is not in the registry, for a context presenting to a
  // native window of the caller's (see
  // RNWebGPU::MakeWebGPUCanvasContextFromNativeWindow): the context owns it,
  // no view attaches to it, and it owns its window (see the SurfaceInfo
  // constructor). It is still tracked for releaseSurfacesForDevice.
  std::shared_ptr<SurfaceInfo>
  makeUnregisteredSurfaceInfo(wgpu::Instance gpu, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    return makeSurfaceInfoLocked(std::move(gpu), width, height,
                                 /* ownsWindow = */ true);
  }

  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // See SurfaceInfo::releaseSurfaceForDevice. Lock order is registry ->
  // SurfaceInfo, matching every other path.
  void releaseSurfacesForDevice(const wgpu::Device &device) {
    std::vector<std::shared_ptr<SurfaceInfo>> infos;
    {
      // Walking _allSurfaces, not _registry: a GPUCanvasContext keeps its own
      // shared_ptr, so an entry erased by clear() (dev reload) or by
      // removeSurfaceInfo() can still be alive and still hold a swapchain for
      // this device.
      std::unique_lock<std::shared_mutex> lock(_mutex);
      pruneAllSurfacesLocked();
      infos.reserve(_allSurfaces.size());
      for (auto &weak : _allSurfaces) {
        if (auto info = weak.lock()) {
          infos.push_back(std::move(info));
        }
      }
    }
    for (auto &info : infos) {
      info->releaseSurfaceForDevice(device);
    }
  }

  // Drops all entries. Called when the RN instance tears down (dev reload):
  // the canvases of the old bundle never run their JS cleanup, and nothing
  // reaches their ids again (see allocateContextId), so the entries would
  // otherwise stay for the life of the process.
  void clear() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.clear();
    _retired.clear();
  }

private:
  SurfaceRegistry() = default;

  std::shared_ptr<SurfaceInfo> getSurfaceInfoOrCreateLocked(int id,
                                                            wgpu::Instance gpu,
                                                            int width,
                                                            int height) {
    auto it = _registry.find(id);
    if (it != _registry.end()) {
      return it->second;
    }
    // A retired entry whose GPUCanvasContext still holds the SurfaceInfo is
    // owned again (see removeSurfaceInfoIfDetached).
    std::shared_ptr<SurfaceInfo> info;
    auto retired = _retired.find(id);
    if (retired != _retired.end()) {
      info = retired->second.lock();
      _retired.erase(retired);
    }
    if (!info) {
      info = makeSurfaceInfoLocked(std::move(gpu), width, height);
    }
    _registry[id] = info;
    return info;
  }

  // Every SurfaceInfo is made here, so releaseSurfacesForDevice reaches it
  // whether or not it is in _registry.
  std::shared_ptr<SurfaceInfo> makeSurfaceInfoLocked(wgpu::Instance gpu,
                                                     int width, int height,
                                                     bool ownsWindow = false) {
    auto info = std::make_shared<SurfaceInfo>(std::move(gpu), width, height,
                                              ownsWindow);
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    pruneAllSurfacesLocked();
    _allSurfaces.push_back(info);
    return info;
  }

  void pruneRetiredLocked() {
    for (auto it = _retired.begin(); it != _retired.end();) {
      it = it->second.expired() ? _retired.erase(it) : ++it;
    }
  }

  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  void pruneAllSurfacesLocked() {
    _allSurfaces.erase(
        std::remove_if(_allSurfaces.begin(), _allSurfaces.end(),
                       [](const std::weak_ptr<SurfaceInfo> &weak) {
                         return weak.expired();
                       }),
        _allSurfaces.end());
  }

  mutable std::shared_mutex _mutex;
  std::atomic<int> _nextContextId{1};
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
  // The entries JS retired while a GPUCanvasContext still held their
  // SurfaceInfo (see removeSurfaceInfoIfDetached). Non-owning; pruned on
  // every retire.
  std::unordered_map<int, std::weak_ptr<SurfaceInfo>> _retired;
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // Every live SurfaceInfo, including the ones no longer in _registry.
  // Non-owning; pruned on insert and on every device destroy.
  std::vector<std::weak_ptr<SurfaceInfo>> _allSurfaces;
};

} // namespace rnwgpu
