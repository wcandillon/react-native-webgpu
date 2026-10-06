#pragma once

#if defined(__ANDROID__)

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "FramePresenter.h"

#include <android/hardware_buffer.h>
#include <android/log.h>
#include <poll.h>
#include <unistd.h>

namespace rnwgpu {

#define RNWGPU_LOG_TAG "WebGPUHardwareBufferView"
// Failure / anomaly reporting only (never per-frame).
#define RNWGPU_POOL_WARN(...)                                                  \
  __android_log_print(ANDROID_LOG_WARN, RNWGPU_LOG_TAG, __VA_ARGS__)

enum class SlotState : uint8_t {
  Free,      // available for getCurrentTexture
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

// The AHB-pool presentation backend behind WebGPUHardwareBufferView: a third
// way to present besides the on-screen wgpu::Surface swapchain and the
// offscreen texture. WebGPU renders into a small pool of native
// AHardwareBuffers imported as Dawn SharedTextureMemory; the view draws each
// finished buffer inline via Bitmap.wrapHardwareBuffer so it behaves like a
// normal RN view. The pool is sized from the JS canvas drawing buffer and
// reallocated when that changes, exactly like the swapchain, so the canvas
// texture always matches the app's other attachments (e.g. its depth texture).
//
// Owned by SurfaceInfo, which decides when the pool is the active presentation
// mode and drives the producer side (the FramePresenter interface) from the
// rendering thread. The consumer side (pollReady, bufferForDisplay,
// releaseSlot, setFrameReadyCallback) is called by the view through the JNI
// layer, see cpp-adapter.cpp.
//
// The pool cannot serve every configuration: it needs a device with AHB
// sharing, an rgba8unorm canvas without view formats, and usages the imported
// buffers support. When it cannot, getCurrentTexture() returns null and the
// unsupported callback tells the view, which switches to the BlitPresenter.
//
// Everything is guarded by _mutex. SurfaceInfo may call in while holding its
// own mutex (lock order SurfaceInfo -> presenter), except for
// getCurrentTexture(), which can block waiting for a free slot.
class HardwareBufferPresenter : public FramePresenter {
public:
  // The view's callbacks, handed back by disable() so the caller can destroy
  // them outside its own lock: they release JNI references.
  struct Callbacks {
    std::function<void()> frameReady;
    std::function<void()> unsupported;
  };

  HardwareBufferPresenter() = default;
  HardwareBufferPresenter(const HardwareBufferPresenter &) = delete;
  HardwareBufferPresenter &operator=(const HardwareBufferPresenter &) = delete;

  ~HardwareBufferPresenter() override { shutdown(); }

  // --- Mode
  // -------------------------------------------------------------------

  // True while the pool is the context's presentation mode (a
  // WebGPUHardwareBufferView is attached).
  bool isEnabled() const override { return _enabled.load(); }

  // Turn on pool mode. The buffers are allocated lazily by resize().
  void enable() {
    std::lock_guard<std::mutex> lock(_mutex);
    _enabled.store(true);
    _shutdown = false;
    // A view just attached: if the pool cannot serve the current
    // configuration, resize() has to find out and tell this view too.
    _unsupported = false;
  }

  // Leave pool mode (the WebGPUHardwareBufferView detached) and drop the pool.
  Callbacks disable() {
    _enabled.store(false);
    std::lock_guard<std::mutex> lock(_mutex);
    dropPoolLocked();
    return {std::exchange(_frameReady, nullptr),
            std::exchange(_unsupportedCallback, nullptr)};
  }

  // Stop the fence waiter before any pool generation is freed, then free them
  // all. Idempotent.
  void shutdown() {
    stopWaiter();
    std::lock_guard<std::mutex> lock(_mutex);
    _presentQueue.clear();
    resetLocked();
    _device = nullptr;
  }

  // --- Configuration (rendering thread)
  // ---------------------------------------

  void configure(const wgpu::SurfaceConfiguration &config) override {
    std::unique_lock<std::mutex> lock(_mutex);
    if (_device.Get() != config.device.Get()) {
      // Buffers imported for another device cannot be reused.
      releaseDeviceObjectsLocked(lock);
    }
    _device = config.device;
    _format = config.format;
    _usage = config.usage;
    _hasViewFormats = config.viewFormatCount > 0;
    // A new configuration may be one the pool supports even if the previous
    // one was not.
    _unsupported = false;
  }

  void unconfigure() override {
    std::unique_lock<std::mutex> lock(_mutex);
    releaseDeviceObjectsLocked(lock);
    _device = nullptr;
  }

  // Called right before `device` is destroyed. The pool's SharedTextureMemory
  // imports belong to the device: drop them while it is alive. The next
  // getCurrentTexture() after a configure with a live device reallocates the
  // pool.
  void releaseForDevice(const wgpu::Device &device) override {
    std::unique_lock<std::mutex> lock(_mutex);
    if (_device && _device.Get() == device.Get()) {
      releaseDeviceObjectsLocked(lock);
      _device = nullptr;
    }
  }

  // --- Producer (rendering thread)
  // --------------------------------------------

  // (Re)allocate the pool to match the canvas drawing buffer (px). Called on
  // the rendering thread before acquiring a slot, so the canvas texture size
  // always tracks the app's other attachments.
  void resize(int pxW, int pxH) override {
    std::function<void()> unsupported;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      if (!_enabled.load() || _device == nullptr || pxW <= 0 || pxH <= 0) {
        return;
      }
      if (_pool != nullptr && _pool->width == pxW && _pool->height == pxH) {
        return;
      }
      if (_unsupported) {
        return;
      }
      std::shared_ptr<AHBPool> pool;
      if (const char *reason = unsupportedReasonLocked()) {
        RNWGPU_POOL_WARN("%s; presenting with a copy instead", reason);
      } else {
        pool = allocatePoolLocked(pxW, pxH); // reports its own failures
      }
      if (pool != nullptr) {
        _pool = pool;
        _pools[pool->generation] = pool;
        // Keep current + previous generation (in-flight frames of the previous
        // size may still be on screen during the cross-fade); drop anything
        // older.
        for (auto it = _pools.begin(); it != _pools.end();) {
          if (it->first + 1 < pool->generation) {
            it = _pools.erase(it);
          } else {
            ++it;
          }
        }
        _freeCv.notify_all();
        return;
      }
      // Sticky until the next configure(). A pool of another size must not
      // keep handing out textures: they would not match the app's other
      // attachments.
      _unsupported = true;
      dropPoolLocked();
      unsupported = _unsupportedCallback;
    }
    if (unsupported) {
      unsupported(); // outside the lock: it calls into Java
    }
  }

  // The texture to render the current frame into, or null when no slot is
  // available (pool not allocated yet, unsupported device, view detached
  // mid-frame, or a free-slot timeout).
  wgpu::Texture getCurrentTexture() override {
    std::unique_lock<std::mutex> lock(_mutex);
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
      RNWGPU_POOL_WARN("getCurrentTexture: timed out waiting for a free slot; "
                       "skipping frame");
      return nullptr;
    }
    if (idx < 0 || _shutdown || pool != _pool) {
      return nullptr;
    }
    auto &slot = pool->slots[idx];
    slot.state = SlotState::Rendering;
    _renderPool = pool;
    _currentRenderSlot = idx;
    beginAccess(slot);
    return slot.texture;
  }

  // Ends access on the in-flight slot and hands it to the fence waiter; the
  // view picks it up once its render-complete fence signals.
  void present() override {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_currentRenderSlot < 0 || _renderPool == nullptr) {
      return;
    }
    queuePresentLocked();
  }

  // --- Consumer (UI thread, called from the JNI layer)
  // ------------------------

  // Latest signaled frame awaiting display, encoded as (generation << 32 |
  // slot). Returns -1 when there is nothing new. Marks the slot Displayed so
  // the producer will not re-render into it until releaseSlot() is called.
  // Called on the UI thread after the frame-ready wake-up.
  int64_t pollReady() {
    std::lock_guard<std::mutex> lock(_mutex);
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
  void *bufferForDisplay(uint32_t gen, int slot) {
    std::lock_guard<std::mutex> lock(_mutex);
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
  void releaseSlot(uint32_t gen, int idx) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_pool != nullptr && _pool->generation == gen && idx >= 0 &&
        idx < static_cast<int>(_pool->slots.size())) {
      _pool->slots[idx].state = SlotState::Free;
      _freeCv.notify_one();
    }
  }

  // Invoked from the waiter thread (never under _mutex) each time a new frame
  // becomes ready, so the consumer can wake up instead of polling every vsync.
  // Pass nullptr to unregister.
  void setFrameReadyCallback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(_mutex);
    _frameReady = std::move(cb);
  }

  // Invoked from the rendering thread (never under _mutex) when the pool turns
  // out to be unusable for the current configuration, so the view can switch
  // to another way of presenting. Pass nullptr to unregister.
  void setUnsupportedCallback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lock(_mutex);
    _unsupportedCallback = std::move(cb);
  }

private:
  // All *Locked helpers below require _mutex to be held.

  // Drop every pool generation and the ready / in-flight slot bookkeeping.
  // Generations pinned by a queued present survive through their
  // PendingPresent until the waiter is done with them.
  void resetLocked() {
    _pool = nullptr;
    _pools.clear();
    _readyPool = nullptr;
    _readySlot = -1;
    _renderPool = nullptr;
    _currentRenderSlot = -1;
    _freeCv.notify_all();
  }

  // Let go of every Dawn object that belongs to _device, on the calling
  // thread, before returning: the caller is about to drop or destroy the
  // device. That includes the frames the waiter still holds. Left alone, the
  // waiter would release their textures later from its own thread, possibly
  // while the device is being destroyed, which Dawn does not survive.
  // Releases `lock` while waiting for the waiter.
  void releaseDeviceObjectsLocked(std::unique_lock<std::mutex> &lock) {
    // Make the waiter give up its fence wait and discard whatever it takes.
    _discardWaits = true;
    do {
      // Frames not picked up by the waiter yet: dropped here, fences included.
      _presentQueue.clear();
      // The frame the waiter is on, if any: wait until it let go of it.
      _waiterIdleCv.wait(lock, [&]() { return !_waiterBusy; });
      // Another thread may have presented while the lock was released.
    } while (!_presentQueue.empty());
    _discardWaits = false;
    resetLocked();
  }

  // Drop the pool, first ending access on a frame that is still being
  // rendered.
  void dropPoolLocked() {
    // A frame between getCurrentTexture and present still has an open
    // BeginAccess; end it and queue it so the waiter drains its fences before
    // the generation is freed (present() will not be called for it).
    if (_renderPool != nullptr && _currentRenderSlot >= 0 &&
        _renderPool->slots[_currentRenderSlot].state == SlotState::Rendering) {
      queuePresentLocked();
    }
    // _presentQueue is intentionally left alone: the waiter still has to wait
    // on the queued fences before the generations they pin can be released; it
    // discards them on publish (no live pool).
    resetLocked();
  }

  // Why the pool cannot serve the current configuration, as far as can be
  // told before allocating; null when it can.
  const char *unsupportedReasonLocked() {
    if (!_device.HasFeature(
            wgpu::FeatureName::SharedTextureMemoryAHardwareBuffer)) {
      return "device lacks SharedTextureMemoryAHardwareBuffer";
    }
    // The buffers are rgba8unorm, the preferred canvas format on Android.
    if (_format != wgpu::TextureFormat::RGBA8Unorm) {
      return "hardware buffers cannot back a canvas format other than "
             "rgba8unorm";
    }
    if (_hasViewFormats) {
      return "hardware buffers cannot back a canvas with viewFormats";
    }
    return nullptr;
  }

  // Allocate + import one generation of the pool. Null when the buffers cannot
  // be allocated, imported, or used the way the canvas is configured.
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
        RNWGPU_POOL_WARN("AHardwareBuffer_allocate failed (%d) %dx%d; "
                         "presenting with a copy instead",
                         err, w, h);
        return nullptr;
      }
      wgpu::SharedTextureMemoryDescriptor memDesc{};
      wgpu::SharedTextureMemoryAHardwareBufferDescriptor ahbDesc{};
      ahbDesc.handle = slot.ahb;
      memDesc.nextInChain = &ahbDesc;
      slot.memory = _device.ImportSharedTextureMemory(&memDesc);
      // A failed import yields a non-null Dawn error object, so probing the
      // properties is the actual validity check.
      wgpu::SharedTextureMemoryProperties props{};
      if (slot.memory == nullptr ||
          slot.memory.GetProperties(&props) != wgpu::Status::Success) {
        RNWGPU_POOL_WARN("ImportSharedTextureMemory failed (%dx%d); presenting "
                         "with a copy instead",
                         w, h);
        return nullptr;
      }
      // The texture must have every usage the canvas was configured with.
      wgpu::TextureUsage usage = _usage | wgpu::TextureUsage::RenderAttachment;
      if ((usage & props.usage) != usage) {
        RNWGPU_POOL_WARN("hardware buffers support usage 0x%llx, not the "
                         "configured 0x%llx; presenting with a copy instead",
                         static_cast<unsigned long long>(props.usage),
                         static_cast<unsigned long long>(usage));
        return nullptr;
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

  // Ends access on the in-flight render slot and hands it (with its fences) to
  // the waiter. The caller guarantees _renderPool / _currentRenderSlot are
  // valid.
  void queuePresentLocked() {
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

  void beginAccess(PoolSlot &slot) {
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
      std::lock_guard<std::mutex> lock(_mutex);
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
  // up via pollReady, which happens strictly after this wait returns.
  void waiterLoop() {
    while (true) {
      PendingPresent pending;
      {
        std::unique_lock<std::mutex> lock(_mutex);
        _presentCv.wait(lock,
                        [&]() { return _shutdown || !_presentQueue.empty(); });
        if (_shutdown && _presentQueue.empty()) {
          return;
        }
        pending = std::move(_presentQueue.front());
        _presentQueue.pop_front();
        _waiterBusy = true;
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
          // teardown (which flips _shutdown) cannot hang here on a wedged GPU,
          // and so releaseDeviceObjectsLocked() gets the frame back promptly.
          struct pollfd pfd;
          pfd.fd = fdInfo.handle;
          pfd.events = POLLIN;
          while (!_shutdown.load() && !_discardWaits.load()) {
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
        std::lock_guard<std::mutex> lock(_mutex);
        // Pool torn down (disable(), or the device is going away) while this
        // frame was in flight: discard it. Publishing it would pin the retired
        // generation forever (nothing polls anymore).
        if (_pool != nullptr && !_discardWaits) {
          // Supersede a still-unclaimed ready frame: drop it back to Free if
          // it belongs to the live pool (a retired pool is simply discarded).
          if (_readyPool != nullptr && _readySlot >= 0) {
            if (_readyPool == _pool) {
              _readyPool->slots[_readySlot].state = SlotState::Free;
              _freeCv.notify_one();
            }
          }
          pending.pool->slots[pending.slot].state = SlotState::Ready;
          _readyPool = pending.pool;
          _readySlot = pending.slot;
          frameReady = _frameReady;
        }
      }
      // Let go of the frame (its fences, and possibly the last reference to
      // its generation) before reporting idle: releaseDeviceObjectsLocked()
      // relies on the waiter holding nothing once it is idle.
      pending = {};
      {
        std::lock_guard<std::mutex> lock(_mutex);
        _waiterBusy = false;
        _waiterIdleCv.notify_all();
      }
      if (frameReady) {
        frameReady(); // outside the lock: it calls into Java
      }
    }
  }

  struct PendingPresent {
    std::shared_ptr<AHBPool> pool;
    int slot;
    std::vector<wgpu::SharedFence> fences;
  };

  static constexpr int kPoolSize =
      5; // 2 held + 1 rendering + 1 waiting + 1 ready

  std::atomic<bool> _enabled{false};
  std::mutex _mutex;
  std::condition_variable _freeCv;    // a slot returned to Free
  std::condition_variable _presentCv; // a frame was queued for the waiter
  // The waiter holds a frame (between taking it from _presentQueue and letting
  // go of it), see releaseDeviceObjectsLocked().
  bool _waiterBusy = false;
  std::condition_variable _waiterIdleCv; // the waiter let go of its frame
  // The waiter must give up its fence wait and discard its frame.
  std::atomic<bool> _discardWaits{false};
  wgpu::Device _device = nullptr;
  wgpu::TextureFormat _format = wgpu::TextureFormat::Undefined;
  wgpu::TextureUsage _usage = wgpu::TextureUsage::RenderAttachment;
  bool _hasViewFormats = false;
  // The pool cannot serve the current configuration (see resize()).
  bool _unsupported = false;
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
  std::function<void()> _frameReady;          // consumer wake-up, see setter
  std::function<void()> _unsupportedCallback; // see setter
};

} // namespace rnwgpu

#endif // defined(__ANDROID__)
