#pragma once

#include <algorithm>
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
// format. Implemented in apple/MetalLayerColorSpace.mm.
void applyCAMetalLayerColorSpace(void *nativeSurface,
                                 wgpu::TextureFormat format);
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
// --- AHB-pool presentation mode (WebGPUHardwareBufferView)
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

#define RNWGPU_LOG_TAG "WebGPUHardwareBufferView"
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

// Bridges the asynchronous native surface lifecycle (surfaces appear and
// disappear on the platform UI thread) with the synchronous WebGPU canvas API
// (the JS render loop must always be able to acquire a texture).
//
// Ownership & threading model:
// - A registry entry is created on first use (by whichever of JS/native gets
//   there first) and lives exactly as long as its JS Canvas: contextIds are
//   never reused. It is removed by the native view's teardown (MetalView
//   dealloc / WebGPUViewManager.onDropViewInstance) when a surface is
//   attached, or by RNWebGPU.destroyContext (the Canvas unmount cleanup) when
//   none ever was — see RNWebGPU::destroyContext for why the split. Surface
//   destruction alone (backgrounding, TextureView teardown) never removes an
//   entry; it only detaches the surface.
// - Attaching a surface is LATCHED: the UI thread stores it as pending
//   (attachSurface) and it is adopted at the next frame boundary — start of
//   getCurrentTexture or end of presentFrame — on whichever thread renders
//   (main JS, Reanimated UI, or a worklet runtime). This preserves Dawn
//   surface thread-affinity and guarantees a surface is never swapped in the
//   middle of a frame. For contexts that are not actively rendering,
//   RNWebGPUManager::flushPendingSurfaceTransition applies the attach from the
//   JS thread instead.
// - Detaching (switchToOffscreen) is IMMEDIATE, because the platform destroys
//   the surface as soon as its callback returns. A configured context falls
//   back to rendering into an offscreen texture, so a running render loop
//   keeps working; the in-flight frame, if any, is dropped at present(). When
//   a new surface attaches, the latest offscreen frame is blitted onto it so
//   content appears without waiting for the next render — the same mechanism
//   that gives a fast time-to-first-frame when rendering starts before the
//   native surface exists.
class SurfaceInfo {
public:
  SurfaceInfo(wgpu::Instance gpu, int width, int height)
      : _gpu(std::move(gpu)), _width(width), _height(height) {}

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
    // Drop the Dawn objects before releasing the native surfaces they borrow.
    _surface = nullptr;
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    _surfaceDevice = nullptr;
#endif
    _pendingSurface = nullptr;
    _texture = nullptr;
    if (_pendingReleaser && _pendingNativeSurface) {
      _pendingReleaser(_pendingNativeSurface);
    }
    if (_releaser && _nativeSurface) {
      _releaser(_nativeSurface);
    }
  }

  // --- Platform UI thread ---------------------------------------------------

  // Store a newly created on-screen surface. It becomes active at the next
  // frame boundary (applyPendingAttach); callers should follow up with
  // RNWebGPUManager::flushPendingSurfaceTransition so contexts that are not
  // currently rendering also pick it up.
  void attachSurface(void *nativeSurface, wgpu::Surface surface,
                     NativeSurfaceReleaser releaser) {
    void *replacedSurface = nullptr;
    NativeSurfaceReleaser replacedReleaser;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_hasPendingAttach) {
        // Replaced before it was ever adopted.
        replacedSurface = _pendingNativeSurface;
        replacedReleaser = std::move(_pendingReleaser);
      }
      _hasPendingAttach = true;
      _pendingNativeSurface = nativeSurface;
      _pendingSurface = std::move(surface);
      _pendingReleaser = std::move(releaser);
    }
    if (replacedReleaser && replacedSurface) {
      replacedReleaser(replacedSurface);
    }
  }

  // The platform surface is being destroyed: detach immediately. If the
  // context is configured, rendering continues into an offscreen texture whose
  // content is blitted to the next attached surface; present() no-ops until
  // then. Safe to call when already offscreen.
  void switchToOffscreen() { detach(/* createFallbackTexture = */ true); }

  // Detach without creating the offscreen fallback: used when the context is
  // being destroyed and nothing will consume further frames.
  void detachSurface() { detach(/* createFallbackTexture = */ false); }

  // Reflects native view layout changes. Does not resize the drawing buffer:
  // that tracks canvas.width/height (like on the web), see
  // GPUCanvasContext::getCurrentTexture.
  void resize(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _width = newWidth;
    _height = newHeight;
  }

  // --- Frame boundary (rendering thread, or the JS thread via
  // RNWebGPUManager::flushPendingSurfaceTransition) ---------------------------

  // Adopt a pending surface if no frame is in flight: configure it and, if
  // frames were rendered offscreen, blit the most recent one onto it and
  // present it, so content shows up without waiting for the render loop.
  // Safe to call from any thread; no-ops when there is nothing pending.
  //
  // supersedeInFlightFrame is set by the rendering thread when it starts a new
  // frame: a previous frame that never presented is abandoned and must not
  // block adoption. The flush path (other threads) leaves it false so it never
  // swaps the surface under a frame that is genuinely in flight.
  void applyPendingAttach(bool supersedeInFlightFrame = false) {
    bool presentBlit = false;
    uint64_t blitEpoch = 0;
    wgpu::Device device = nullptr;
    void *replacedSurface = nullptr;
    NativeSurfaceReleaser replacedReleaser;
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (supersedeInFlightFrame) {
        _frameInFlight = false;
        _acquiredFromSurface = false;
      }
      if (!_hasPendingAttach || _frameInFlight) {
        return;
      }
      // Attach over attach without a detach in between: replace. Ownership
      // tracks the native window pointer, not the Dawn surface handle (which
      // can be null if surface creation failed).
      replacedSurface = _nativeSurface;
      replacedReleaser = std::move(_releaser);
      _surface = std::move(_pendingSurface);
#if defined(__ANDROID__)
      // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
      _surfaceDevice = nullptr;
#endif
      _nativeSurface = _pendingNativeSurface;
      _releaser = std::move(_pendingReleaser);
      _hasPendingAttach = false;
      _pendingNativeSurface = nullptr;
      _frameEpoch++;

      // _surface can be null here when Dawn surface creation failed for a
      // valid native window; the context then just keeps rendering offscreen.
      if (_config.device != nullptr && _surface) {
        bool blit = _texture != nullptr;
        // The blit needs CopyDst on the surface. Configure with a widened
        // copy while keeping _config at the usage the user asked for, so any
        // later reconfigure drops the extra flag again.
        wgpu::SurfaceConfiguration config = _config;
        if (blit) {
          config.usage |= wgpu::TextureUsage::CopyDst;
        }
        configureSurfaceLocked(config);
#ifdef __APPLE__
        applyCAMetalLayerColorSpace(_nativeSurface, _config.format);
#endif
        if (blit) {
          presentBlit = blitOffscreenToSurfaceLocked();
          device = _config.device;
          // Consumed either way; on failure the next frame renders fresh.
          _texture = nullptr;
          blitEpoch = _frameEpoch;
        }
      }
    }
    if (replacedReleaser && replacedSurface) {
      replacedReleaser(replacedSurface);
    }
    if (presentBlit) {
#ifdef __APPLE__
      if (device) {
        dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
      }
#endif
      std::unique_lock<std::shared_mutex> lock(_mutex);
      // Present only if the blitted texture is still the surface's current
      // one. The epoch changes on any acquire, present, configure, detach, or
      // adoption, so a frame that started - even one that already completed -
      // or any other transition while we were unlocked skips this present
      // (their newer content stands; presenting here would be a Dawn
      // present-without-acquire error).
      if (_surface && !_frameInFlight && _frameEpoch == blitEpoch) {
        _surface.Present();
      }
    }
  }

  // --- Rendering thread
  // -------------------------------------------------------

  void configure(wgpu::SurfaceConfiguration &newConfig,
                 std::vector<wgpu::TextureFormat> viewFormats) {
    applyPendingAttach(/* supersedeInFlightFrame = */ true);
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _viewFormats = std::move(viewFormats);
    _config = newConfig;
    // The caller's viewFormats storage dies with the call; point the stored
    // configuration at our own copy.
    _config.viewFormats = _viewFormats.empty() ? nullptr : _viewFormats.data();
    _config.viewFormatCount = _viewFormats.size();
    // The drawing buffer starts at the canvas size. Clamp so a canvas that has
    // not been laid out yet (0x0) configures instead of erroring.
    _config.width = std::max(1, _width);
    _config.height = std::max(1, _height);
    _texture = nullptr;
    _frameEpoch++;
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
    _configureLocked();
  }

  // Resize the drawing buffer (canvas.width/height changed).
  void reconfigure(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      return;
    }
    _config.width = std::max(1, newWidth);
    _config.height = std::max(1, newHeight);
    _texture = nullptr;
    _frameEpoch++;
    _configureLocked();
  }

  void unconfigure() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_surface) {
      _surface.Unconfigure();
    }
    _texture = nullptr;
    _config = {};
    _viewFormats.clear();
    _acquiredFromSurface = false;
    _frameEpoch++;
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
  // Remove once the pinned Dawn contains the upstream fix; the check in
  // scripts/install-dawn.ts fails the install when the Dawn pin changes so
  // that decision is not forgotten.
  void releaseSurfaceForDevice(const wgpu::Device &device) {
#if defined(__ANDROID__)
    std::unique_lock<std::shared_mutex> lock(_mutex);
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
    // Left unconfigured: _config still names the destroyed device, and
    // configuring against it would only raise a validation error. The next
    // configure() from JS (or a surface re-attach) configures it.
    recreateSurfaceLocked();
#else
    (void)device;
#endif
  }

  // True while a native view owns a surface for this context (attached or
  // pending adoption). Used to decide which side retires the registry entry:
  // the native view's teardown when a surface exists, the JS Canvas cleanup
  // otherwise (see RNWebGPU::destroyContext).
  bool hasNativeSurface() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
#if defined(__ANDROID__)
    // A WebGPUHardwareBufferView in pool mode owns presentation the same way a
    // surface-backed view does, and retires the entry on its own teardown.
    if (_poolMode.load()) {
      return true;
    }
#endif
    return _nativeSurface != nullptr || _hasPendingAttach;
  }

  // Returns the texture for the current frame: the surface's swapchain texture
  // when a surface is attached and healthy, an offscreen texture otherwise.
  // Never returns null; throws when called before configure().
  wgpu::Texture getCurrentTexture() {
    // Start-of-frame boundary; a new acquire supersedes any previous frame
    // that never presented.
    applyPendingAttach(/* supersedeInFlightFrame = */ true);
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
        _acquiredFromSurface = false;
        _frameEpoch++;
        return texture;
      }
      // No slot available (pool not allocated yet, unsupported device, view
      // detached mid-frame, or a free-slot timeout): fall through to the
      // offscreen fallback so the render loop survives; this frame is simply
      // not shown.
    }
#endif
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (_config.device == nullptr) {
      throw std::runtime_error(
          "[WebGPU] getCurrentTexture() called on a canvas context that is "
          "not configured; call context.configure() first");
    }
    _frameInFlight = true;
    _acquiredFromSurface = false;
    _frameEpoch++;
    if (_surface) {
      auto texture = acquireSurfaceTextureLocked();
      if (texture) {
        _acquiredFromSurface = true;
        return texture;
      }
      // The surface is transiently unusable (e.g. mid-resize, lost while
      // backgrounding): fall back to an offscreen texture so the render loop
      // survives; this frame is simply not presented.
    }
    if (!_texture) {
      _texture = createOffscreenTextureLocked();
    }
    return _texture;
  }

  // Present the current frame. Runs synchronously on the thread that did
  // getCurrentTexture/submit (main JS, Reanimated UI, or a worklet runtime),
  // preserving Dawn surface thread-affinity. Frames whose texture was not
  // acquired from the attached surface (offscreen, detached mid-frame, or
  // acquire failure) are dropped. This is also the end-of-frame boundary: it
  // adopts a surface that attached while the frame was in flight.
  void presentFrame() {
#if defined(__ANDROID__)
    if (_poolMode.load()) {
      // Ends access on the pool slot and hands it to the fence waiter; the
      // view picks it up once its render-complete fence signals.
      poolPresent();
    }
#endif
#ifdef __APPLE__
    // Ensure command buffers are scheduled before presenting. Read the device
    // under a shared lock, then wait without holding it (the wait can block).
    wgpu::Device device;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      device = _config.device;
    }
    if (device) {
      dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
    }
#endif
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      if (_surface && _acquiredFromSurface) {
        _surface.Present();
      }
      _acquiredFromSurface = false;
      _frameInFlight = false;
      _frameEpoch++;
    }
    applyPendingAttach();
  }

  NativeInfo getNativeInfo() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    // A surface that is still pending adoption is the one callers should see.
    void *native = _hasPendingAttach ? _pendingNativeSurface : _nativeSurface;
    return {.nativeSurface = native, .width = _width, .height = _height};
  }

  Size getSize() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return {.width = _width, .height = _height};
  }

  wgpu::SurfaceConfiguration getConfig() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return _config;
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
#endif

private:
  void detach(bool createFallbackTexture) {
    void *releasedSurfaces[2] = {nullptr, nullptr};
    NativeSurfaceReleaser releasers[2];
#if defined(__ANDROID__)
    // Destroyed outside the lock: it releases a JNI reference.
    std::function<void()> releasedFrameReady;
#endif
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
#if defined(__ANDROID__)
      // Leaving pool mode (the WebGPUHardwareBufferView detached): drop the
      // pool. A configured context keeps rendering into the offscreen
      // fallback, exactly like the surface path.
      if (_poolMode.load()) {
        _poolMode.store(false);
        {
          std::lock_guard<std::mutex> poolLock(_poolMutex);
          // A frame between getCurrentTexture and present still has an open
          // BeginAccess; end it and queue it so the waiter drains its fences
          // before the generation is freed (presentFrame() no-ops for it
          // because pool mode is already off).
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
        if (createFallbackTexture && _config.device != nullptr && !_texture) {
          _texture = createOffscreenTextureLocked();
        }
        _frameEpoch++;
      }
#endif
      // The platform is tearing surfaces down; a not-yet-adopted attach is
      // stale, cancel it.
      if (_hasPendingAttach) {
        _hasPendingAttach = false;
        _pendingSurface = nullptr;
        releasedSurfaces[0] = _pendingNativeSurface;
        releasers[0] = std::move(_pendingReleaser);
        _pendingNativeSurface = nullptr;
      }
      if (_surface) {
        if (createFallbackTexture && _config.device != nullptr) {
          _texture = createOffscreenTextureLocked();
        }
        _surface = nullptr;
#if defined(__ANDROID__)
        // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
        _surfaceDevice = nullptr;
#endif
        // The in-flight frame (if any) rendered into the destroyed surface;
        // presentFrame() must not present it.
        _acquiredFromSurface = false;
      }
      _frameEpoch++;
      // Window ownership is independent of the Dawn surface handle (which can
      // be null if surface creation failed): always return the window.
      releasedSurfaces[1] = _nativeSurface;
      releasers[1] = std::move(_releaser);
      _nativeSurface = nullptr;
    }
    // Release outside the lock: the platform may do real work here.
    for (int i = 0; i < 2; i++) {
      if (releasers[i] && releasedSurfaces[i]) {
        releasers[i](releasedSurfaces[i]);
      }
    }
  }

  // All *Locked helpers below require _mutex to be held exclusively.

  wgpu::Texture createOffscreenTextureLocked() {
    wgpu::TextureDescriptor descriptor;
    // Union with the user's usage so offscreen frames stay compatible with
    // whatever they configured (e.g. CopySrc readbacks). RenderAttachment |
    // CopySrc | TextureBinding is what the fallback itself needs (rendering,
    // the attach blit, sampling).
    descriptor.usage = _config.usage | wgpu::TextureUsage::RenderAttachment |
                       wgpu::TextureUsage::CopySrc |
                       wgpu::TextureUsage::TextureBinding;
    descriptor.format = _config.format;
    descriptor.size.width = std::max(1u, _config.width);
    descriptor.size.height = std::max(1u, _config.height);
    descriptor.viewFormats = _config.viewFormats;
    descriptor.viewFormatCount = _config.viewFormatCount;
    return _config.device.CreateTexture(&descriptor);
  }

  // Acquire the surface's current texture, reconfiguring once when the surface
  // reports it is stale (rotation, resize, coming back from background).
  wgpu::Texture acquireSurfaceTextureLocked() {
    wgpu::SurfaceTexture surfaceTexture;
    _surface.GetCurrentTexture(&surfaceTexture);
    if (!isAcquireSuccess(surfaceTexture)) {
      if (surfaceTexture.status ==
          wgpu::SurfaceGetCurrentTextureStatus::Error) {
        return nullptr;
      }
      configureSurfaceLocked(_config);
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

  // Copy the last offscreen frame onto the freshly attached surface. Returns
  // true when the copy was submitted and the surface should be presented.
  bool blitOffscreenToSurfaceLocked() {
    wgpu::SurfaceTexture surfaceTexture;
    _surface.GetCurrentTexture(&surfaceTexture);
    if (!isAcquireSuccess(surfaceTexture)) {
      return false;
    }

    wgpu::TexelCopyTextureInfo source = {};
    source.texture = _texture;
    wgpu::TexelCopyTextureInfo destination = {};
    destination.texture = surfaceTexture.texture;

    // The offscreen frame and the new surface can disagree on size (e.g. the
    // device rotated while detached); copy the shared region.
    wgpu::Extent3D size = {
        std::min(_texture.GetWidth(), surfaceTexture.texture.GetWidth()),
        std::min(_texture.GetHeight(), surfaceTexture.texture.GetHeight()), 1};

    wgpu::CommandEncoderDescriptor encoderDescriptor;
    wgpu::CommandEncoder encoder =
        _config.device.CreateCommandEncoder(&encoderDescriptor);
    encoder.CopyTextureToTexture(&source, &destination, &size);
    wgpu::CommandBuffer commands = encoder.Finish();
    _config.device.GetQueue().Submit(1, &commands);
    return true;
  }

  void _configureLocked() {
    if (_surface) {
      configureSurfaceLocked(_config);
#ifdef __APPLE__
      applyCAMetalLayerColorSpace(_nativeSurface, _config.format);
#endif
    } else {
      _texture = createOffscreenTextureLocked();
    }
  }

  // Every Surface::Configure goes through here. Once the
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE lines are gone, this
  // is a plain wrapper around _surface.Configure.
  void configureSurfaceLocked(const wgpu::SurfaceConfiguration &config) {
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    // Vulkan refuses to switch a surface to another device and keeps the old
    // swapchain attached, so a device change (device swap, recovery after a
    // lost device, a re-run effect) is done with a fresh surface. This also
    // tears the old swapchain down while its device is still alive.
    if (_surfaceDevice && _surfaceDevice.Get() != config.device.Get()) {
      recreateSurfaceLocked();
      if (!_surface) {
        return;
      }
    }
#endif
    _surface.Configure(&config);
#if defined(__ANDROID__)
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    _surfaceDevice = config.device;
#endif
  }

#if defined(__ANDROID__)
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // Replace _surface with a new, unconfigured surface for the same native
  // window. Dropping the old one runs ~Surface, which detaches both the
  // active and the recycled swapchain against their still-alive device.
  void recreateSurfaceLocked() {
    _surface = nullptr;
    _surfaceDevice = nullptr;
    // A frame acquired from the old surface can no longer be presented.
    _acquiredFromSurface = false;
    _frameEpoch++;
    if (_nativeSurface) {
      wgpu::SurfaceSourceAndroidNativeWindow source;
      source.window = _nativeSurface;
      wgpu::SurfaceDescriptor descriptor;
      descriptor.nextInChain = &source;
      _surface = _gpu.CreateSurface(&descriptor);
    }
  }
#endif

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
        // Pool torn down (switchToOffscreen) while this frame was in flight:
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
  // Attached on-screen surface (null while offscreen).
  void *_nativeSurface = nullptr;
  wgpu::Surface _surface = nullptr;
  NativeSurfaceReleaser _releaser;
  // Offscreen fallback drawing buffer.
  wgpu::Texture _texture = nullptr;
  // Surface attached by the UI thread, awaiting adoption at a frame boundary.
  bool _hasPendingAttach = false;
  void *_pendingNativeSurface = nullptr;
  wgpu::Surface _pendingSurface = nullptr;
  NativeSurfaceReleaser _pendingReleaser;
  // Frame state: set by getCurrentTexture, cleared by presentFrame.
  bool _frameInFlight = false;
  bool _acquiredFromSurface = false;
  // Bumped on every acquire, present, configure/reconfigure/unconfigure,
  // adoption, and detach. The deferred blit-present in applyPendingAttach
  // revalidates against it so it never presents a texture that stopped being
  // the surface's current one while the lock was released.
  uint64_t _frameEpoch = 0;
  // device == nullptr means "not configured". _viewFormats owns the storage
  // that _config.viewFormats points at.
  wgpu::SurfaceConfiguration _config;
  std::vector<wgpu::TextureFormat> _viewFormats;
  // Keeps the Dawn instance alive for as long as any canvas exists.
  wgpu::Instance _gpu;
  // Native view size in dp (surfaced as clientWidth/clientHeight on the JS
  // canvas).
  int _width;
  int _height;

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
  }

  std::shared_ptr<SurfaceInfo>
  getSurfaceInfoOrCreate(int id, wgpu::Instance gpu, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    return getSurfaceInfoOrCreateLocked(id, gpu, width, height);
  }

  // Find-or-create + attach as one atomic step under the registry lock, so it
  // serializes with removeSurfaceInfoIfDetached: an attach can never land on
  // an entry that a concurrent destroyContext is erasing (it either marks the
  // entry attached before the check, or re-creates the entry after the
  // erase). Lock order is registry -> SurfaceInfo, matching every other path.
  std::shared_ptr<SurfaceInfo> attachSurface(int id, wgpu::Instance gpu,
                                             int width, int height,
                                             void *nativeSurface,
                                             wgpu::Surface surface,
                                             NativeSurfaceReleaser releaser) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto info = getSurfaceInfoOrCreateLocked(id, gpu, width, height);
    info->attachSurface(nativeSurface, std::move(surface), std::move(releaser));
    return info;
  }

  // Erase the entry only if no native surface is attached or pending; the
  // atomic counterpart of attachSurface above (see RNWebGPU::destroyContext
  // for the ownership split this implements).
  void removeSurfaceInfoIfDetached(int id) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(id);
    if (it == _registry.end() || it->second->hasNativeSurface()) {
      return;
    }
    _registry.erase(it);
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
  // JS context ids restart from scratch, so surviving entries would alias new
  // canvases onto dead surfaces.
  void clear() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    _registry.clear();
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
    auto info = std::make_shared<SurfaceInfo>(gpu, width, height);
    _registry[id] = info;
    // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
    pruneAllSurfacesLocked();
    _allSurfaces.push_back(info);
    return info;
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
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
  // DAWN_WORKAROUND_DEVICE_DESTROY_BEFORE_SURFACE_RELEASE
  // Every live SurfaceInfo, including the ones no longer in _registry.
  // Non-owning; pruned on insert and on every device destroy.
  std::vector<std::weak_ptr<SurfaceInfo>> _allSurfaces;
};

} // namespace rnwgpu
