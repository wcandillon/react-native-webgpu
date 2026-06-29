#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
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

struct NativeInfo {
  void *nativeSurface;
  int width;
  int height;
};

struct Size {
  int width;
  int height;
};

#if defined(__ANDROID__)
// --- AHB-pool presentation mode (WebGPUAHBView) -----------------------------
//
// A third presentation backend (besides the on-screen wgpu::Surface swapchain
// and the offscreen texture). WebGPU renders into a small pool of native
// AHardwareBuffers imported as Dawn SharedTextureMemory; the view draws each
// finished buffer inline via Bitmap.wrapHardwareBuffer so it behaves like a
// normal RN view. The pool is sized from the JS canvas drawing buffer
// (_canvas->getWidth()/Height()) and reallocated when that changes, exactly
// like the swapchain, so the canvas texture always matches the app's other
// attachments (e.g. its depth texture).

#define RNWGPU_LOG_TAG "WebGPUAHBView"
#define RNWGPU_POOL_LOG(...)                                                    \
  __android_log_print(ANDROID_LOG_INFO, RNWGPU_LOG_TAG, __VA_ARGS__)

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
// imports. The Java side keeps its own ref (via wrapHardwareBuffer) for anything
// it is still drawing, so a generation can be freed here without disturbing a
// frame still on screen.
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

class SurfaceInfo {
public:
  SurfaceInfo(wgpu::Instance gpu, int width, int height)
      : gpu(std::move(gpu)), width(width), height(height) {}

  ~SurfaceInfo() {
#if defined(__ANDROID__)
    stopWaiter();
#endif
    surface = nullptr;
  }

  void reconfigure(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    config.width = newWidth;
    config.height = newHeight;
    _configure();
  }

  void configure(wgpu::SurfaceConfiguration &newConfig) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    config = newConfig;
    config.width = width;
    config.height = height;
    config.presentMode = wgpu::PresentMode::Fifo;
#if defined(__ANDROID__)
    // Always make the device available to the pool. In pool mode there is no
    // wgpu::Surface and no offscreen texture to build; the pool is allocated
    // lazily from the canvas size in poolResize().
    {
      std::lock_guard<std::mutex> poolLock(_poolMutex);
      _poolDevice = config.device;
    }
    if (_poolMode.load()) {
      return;
    }
#endif
    _configure();
  }

  void unconfigure() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (surface) {
      surface.Unconfigure();
    } else {
      texture = nullptr;
    }
  }

  void *switchToOffscreen() {
    std::unique_lock<std::shared_mutex> lock(_mutex);
#if defined(__ANDROID__)
    // Leaving pool mode (the view detached / went away): keep the canvas alive
    // by falling back to an offscreen texture, exactly like the surface path.
    if (_poolMode.load()) {
      _poolMode.store(false);
      int offW = config.width;
      int offH = config.height;
      {
        std::lock_guard<std::mutex> poolLock(_poolMutex);
        if (_pool != nullptr) {
          offW = _pool->width;
          offH = _pool->height;
        }
        _pool = nullptr;
        _pools.clear();
        _readyPool = nullptr;
        _readySlot = -1;
        _renderPool = nullptr;
        _currentRenderSlot = -1;
        _presentQueue.clear();
        _freeCv.notify_all();
      }
      if (config.device != nullptr && offW > 0 && offH > 0) {
        wgpu::TextureDescriptor textureDesc;
        textureDesc.usage = wgpu::TextureUsage::RenderAttachment |
                            wgpu::TextureUsage::CopySrc |
                            wgpu::TextureUsage::TextureBinding;
        textureDesc.format = config.format;
        textureDesc.size.width = offW;
        textureDesc.size.height = offH;
        texture = config.device.CreateTexture(&textureDesc);
      }
      return nativeSurface;
    }
#endif
    // We only do this if the onscreen surface is configured.
    auto isConfigured = config.device != nullptr;
    if (isConfigured) {
      wgpu::TextureDescriptor textureDesc;
      textureDesc.usage = wgpu::TextureUsage::RenderAttachment |
                          wgpu::TextureUsage::CopySrc |
                          wgpu::TextureUsage::TextureBinding;
      textureDesc.format = config.format;
      textureDesc.size.width = config.width;
      textureDesc.size.height = config.height;
      texture = config.device.CreateTexture(&textureDesc);
    }
    surface = nullptr;
    return nativeSurface;
  }

  void switchToOnscreen(void *newNativeSurface, wgpu::Surface newSurface) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    nativeSurface = newNativeSurface;
    surface = std::move(newSurface);
    // If we are comming from an offscreen context, we need to configure the new
    // surface
    if (texture != nullptr) {
      config.usage = config.usage | wgpu::TextureUsage::CopyDst;
      _configure();
      // We flush the offscreen texture to the onscreen one
      // TODO: there is a faster way to do this without validation?
      wgpu::CommandEncoderDescriptor encoderDesc;
      auto device = config.device;
      wgpu::CommandEncoder encoder = device.CreateCommandEncoder(&encoderDesc);

      wgpu::TexelCopyTextureInfo sourceTexture = {};
      sourceTexture.texture = texture;

      wgpu::TexelCopyTextureInfo destinationTexture = {};
      wgpu::SurfaceTexture surfaceTexture;
      surface.GetCurrentTexture(&surfaceTexture);
      destinationTexture.texture = surfaceTexture.texture;

      wgpu::Extent3D size = {sourceTexture.texture.GetWidth(),
                             sourceTexture.texture.GetHeight(),
                             sourceTexture.texture.GetDepthOrArrayLayers()};

      encoder.CopyTextureToTexture(&sourceTexture, &destinationTexture, &size);

      wgpu::CommandBuffer commands = encoder.Finish();
      wgpu::Queue queue = device.GetQueue();
      queue.Submit(1, &commands);
      surface.Present();
      texture = nullptr;
    }
  }

  void resize(int newWidth, int newHeight) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    width = newWidth;
    height = newHeight;
  }

  // Present the current surface texture. Called synchronously from the thread
  // that did getCurrentTexture / submit (via GPUCanvasContext::present), so it
  // preserves Dawn surface thread-affinity. No-op when offscreen / unconfigured
  // (no surface).
  void presentFrame() {
#if defined(__ANDROID__)
    if (_poolMode.load()) {
      poolPresent();
      return;
    }
#endif
#ifdef __APPLE__
    // Ensure command buffers are scheduled before presenting. Read the device
    // under a shared lock, then wait without holding it (the wait can block).
    // The device may be reconfigured between the two locks; that is safe because
    // present() is called on the rendering thread right after submit(), the wait
    // just flushes that thread's already-submitted work, and the Present() below
    // re-checks `surface` under the unique lock before touching it.
    wgpu::Device device;
    {
      std::shared_lock<std::shared_mutex> lock(_mutex);
      device = config.device;
    }
    if (device) {
      dawn::native::metal::WaitForCommandsToBeScheduled(device.Get());
    }
#endif
    std::unique_lock<std::shared_mutex> lock(_mutex);
    if (surface) {
      surface.Present();
    }
  }

  wgpu::Texture getCurrentTexture() {
#if defined(__ANDROID__)
    if (_poolMode.load()) {
      return poolGetCurrentTexture();
    }
#endif
    std::shared_lock<std::shared_mutex> lock(_mutex);
    if (surface) {
      wgpu::SurfaceTexture surfaceTexture;
      surface.GetCurrentTexture(&surfaceTexture);
      return surfaceTexture.texture;
    } else {
      return texture;
    }
  }

  // True when an on-screen wgpu::Surface is attached (vs offscreen texture).
  bool hasSurface() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return surface != nullptr;
  }

  bool isPoolMode() {
#if defined(__ANDROID__)
    return _poolMode.load();
#else
    return false;
#endif
  }

  NativeInfo getNativeInfo() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return {.nativeSurface = nativeSurface, .width = width, .height = height};
  }

  Size getSize() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return {.width = width, .height = height};
  }

  wgpu::SurfaceConfiguration getConfig() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return config;
  }

  wgpu::Device getDevice() {
    std::shared_lock<std::shared_mutex> lock(_mutex);
    return config.device;
  }

#if defined(__ANDROID__)
  // --- AHB-pool API (called from the JNI layer, see cpp-adapter.cpp) ---------

  // Turn on pool mode for this context. dpW/dpH is the canvas-client (dp) size
  // reported to JS via getSize(); the actual buffers are sized in pool px from
  // the canvas drawing buffer in poolResize().
  void enablePool(int dpW, int dpH) {
    {
      std::unique_lock<std::shared_mutex> lock(_mutex);
      width = dpW;
      height = dpH;
    }
    std::lock_guard<std::mutex> poolLock(_poolMutex);
    _poolMode.store(true);
    _shutdown = false;
  }

  // Set the canvas-client (dp) size without changing pool mode. Keeps getSize()
  // (and thus the JS canvas.clientWidth/Height) in sync on resize.
  void setPoolClientSize(int dpW, int dpH) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    width = dpW;
    height = dpH;
  }

  // (Re)allocate the pool to match the canvas drawing buffer (px). Called from
  // GPUCanvasContext::getCurrentTexture on the JS thread before acquiring a
  // slot, so the canvas texture size always tracks the app's other attachments.
  void poolResize(int pxW, int pxH) {
    std::lock_guard<std::mutex> poolLock(_poolMutex);
    if (!_poolMode.load() || _poolDevice == nullptr || pxW <= 0 || pxH <= 0) {
      return;
    }
    if (_pool != nullptr && _pool->width == pxW && _pool->height == pxH) {
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
    RNWGPU_POOL_LOG("poolResize: gen=%u size=%dx%d", pool->generation, pxW, pxH);
    _freeCv.notify_all();
  }

  // Latest signaled frame awaiting display, encoded as (generation << 32 | slot).
  // Returns -1 when there is nothing new. Marks the slot Displayed so the
  // producer will not re-render into it until releaseSlot() is called. Called on
  // the UI thread from the view's Choreographer callback.
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
#endif

private:
  void _configure() {
    if (surface) {
      surface.Configure(&config);
    } else {
      wgpu::TextureDescriptor textureDesc;
      textureDesc.format = config.format;
      textureDesc.size.width = config.width;
      textureDesc.size.height = config.height;
      textureDesc.usage = wgpu::TextureUsage::RenderAttachment |
                          wgpu::TextureUsage::CopySrc |
                          wgpu::TextureUsage::TextureBinding;
      texture = config.device.CreateTexture(&textureDesc);
    }
  }

#if defined(__ANDROID__)
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
        RNWGPU_POOL_LOG("AHardwareBuffer_allocate failed (%d) %dx%d", err, w, h);
        return nullptr;
      }
      wgpu::SharedTextureMemoryDescriptor memDesc{};
      wgpu::SharedTextureMemoryAHardwareBufferDescriptor ahbDesc{};
      ahbDesc.handle = slot.ahb;
      memDesc.nextInChain = &ahbDesc;
      slot.memory = _poolDevice.ImportSharedTextureMemory(&memDesc);
      if (slot.memory == nullptr) {
        RNWGPU_POOL_LOG("ImportSharedTextureMemory returned null");
        return nullptr;
      }
      // No descriptor: take the buffer's natural format (rgba8unorm) and usage
      // (RenderAttachment, from GPU_COLOR_OUTPUT). The canvas is configured
      // rgba8unorm on Android (the preferred format), so they match.
      slot.texture = slot.memory.CreateTexture();
      slot.state = SlotState::Free;
    }
    return pool;
  }

  wgpu::Texture poolGetCurrentTexture() {
    std::unique_lock<std::mutex> lock(_poolMutex);
    if (_pool == nullptr) {
      RNWGPU_POOL_LOG("poolGetCurrentTexture: no pool (device ready=%d)",
                      _poolDevice != nullptr);
      return nullptr;
    }
    auto pool = _pool;
    int idx = -1;
    _freeCv.wait(lock, [&]() {
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
        _presentCv.wait(lock, [&]() {
          return _shutdown || !_presentQueue.empty();
        });
        if (_shutdown && _presentQueue.empty()) {
          return;
        }
        pending = std::move(_presentQueue.front());
        _presentQueue.pop_front();
      }

      // Wait outside the lock. Each fd is owned by its SharedFence and closed
      // when `pending.fences` is destroyed at the end of this iteration, so we
      // never dup / double-close (avoids the fdsan abort, see GPUSharedFence.cpp).
      for (auto &fence : pending.fences) {
        wgpu::SharedFenceExportInfo info{};
        wgpu::SharedFenceSyncFDExportInfo fdInfo{};
        info.nextInChain = &fdInfo;
        fence.ExportInfo(&info);
        if (fdInfo.handle >= 0) {
          // Sync-fence fds become readable (POLLIN) when signaled; poll() avoids
          // any libsync linkage concern. Poll in bounded slices so teardown
          // (which flips _shutdown) cannot hang here on a wedged GPU.
          struct pollfd pfd;
          pfd.fd = fdInfo.handle;
          pfd.events = POLLIN;
          while (!_shutdown.load()) {
            pfd.revents = 0;
            int r = poll(&pfd, 1, 100);
            if (r != 0) {
              break; // signaled, error, or POLLERR: stop waiting
            }
          }
        }
      }

      {
        std::lock_guard<std::mutex> lock(_poolMutex);
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
      }
    }
  }
#endif

  mutable std::shared_mutex _mutex;
  void *nativeSurface = nullptr;
  wgpu::Surface surface = nullptr;
  wgpu::Texture texture = nullptr;
  wgpu::Instance gpu;
  wgpu::SurfaceConfiguration config;
  int width;
  int height;

#if defined(__ANDROID__)
  // Pool state. Guarded by _poolMutex (never nested under _mutex while blocking).
  struct PendingPresent {
    std::shared_ptr<AHBPool> pool;
    int slot;
    std::vector<wgpu::SharedFence> fences;
  };

  static constexpr int kPoolSize = 5; // 2 held + 1 rendering + 1 waiting + 1 ready

  std::atomic<bool> _poolMode{false};
  std::mutex _poolMutex;
  std::condition_variable _freeCv;    // a slot returned to Free
  std::condition_variable _presentCv; // a frame was queued for the waiter
  wgpu::Device _poolDevice = nullptr;
  std::shared_ptr<AHBPool> _pool;       // current generation
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

  std::shared_ptr<SurfaceInfo> addSurfaceInfo(int id, wgpu::Instance gpu,
                                              int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto info = std::make_shared<SurfaceInfo>(gpu, width, height);
    _registry[id] = info;
    return info;
  }

  std::shared_ptr<SurfaceInfo>
  getSurfaceInfoOrCreate(int id, wgpu::Instance gpu, int width, int height) {
    std::unique_lock<std::shared_mutex> lock(_mutex);
    auto it = _registry.find(id);
    if (it != _registry.end()) {
      return it->second;
    }
    auto info = std::make_shared<SurfaceInfo>(gpu, width, height);
    _registry[id] = info;
    return info;
  }

private:
  SurfaceRegistry() = default;
  mutable std::shared_mutex _mutex;
  std::unordered_map<int, std::shared_ptr<SurfaceInfo>> _registry;
};

} // namespace rnwgpu
