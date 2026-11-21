#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "webgpu/webgpu_cpp.h"

#include "PlatformContext.h"

namespace rnwgpu {

// The color space a NativeBufferView displays a buffer in. The buffer holds
// premultiplied, gamma-encoded values in these primaries. rgba16float gives
// more precision, not extended range: the view shows values in [0, 1] and
// clamps the rest.
enum class NativeBufferColorSpace { SRGB, DisplayP3 };

// A native buffer handed to a NativeBufferView: an AHardwareBuffer* (Android)
// or an IOSurfaceRef (Apple platforms), with a reference of its own, released
// when the last holder (the registry, a pending fence wait, the view) drops
// it. The compositor takes references of its own on what it displays
// (Bitmap.wrapHardwareBuffer, CALayer.contents), so dropping this one while
// the buffer is on screen is safe.
class NativeBufferContent {
public:
  // Validates the frame for display (a single-plane RGBA buffer in a format
  // the view can show) and takes a reference on its buffer. Throws a
  // std::runtime_error naming the problem.
  static std::shared_ptr<NativeBufferContent>
  fromFrame(const VideoFrameHandle &frame);

  ~NativeBufferContent();

  NativeBufferContent(const NativeBufferContent &) = delete;
  NativeBufferContent &operator=(const NativeBufferContent &) = delete;

  void *buffer() const { return _buffer; }
  uint32_t width() const { return _width; }
  uint32_t height() const { return _height; }
  NativeBufferColorSpace colorSpace() const { return _colorSpace; }

private:
  NativeBufferContent(void *buffer, uint32_t width, uint32_t height,
                      NativeBufferColorSpace colorSpace);

  void *_buffer;
  uint32_t _width;
  uint32_t _height;
  NativeBufferColorSpace _colorSpace;
};

// Waits for `fence` to reach `signaledValue` without blocking the caller, then
// calls `done` from another thread: true once the fence signaled, false when
// the wait failed or gave up (the reason is logged). Implemented per platform:
// a sync-fd poll on a waiter thread on Android, an MTLSharedEvent listener on
// Apple platforms.
void waitForSharedFence(wgpu::SharedFence fence, uint64_t signaledValue,
                        std::function<void(bool)> done);

// The buffers presented to each NativeBufferView, keyed by a view id that JS
// allocates (allocateViewId) and passes to the native view as a prop. JS
// presents a buffer from any runtime (presentNativeBuffer); once its fence
// has signaled the buffer becomes the view's content and the view's listener
// is called, from any thread. The view then reads the content on the main
// thread and hands it to the compositor.
//
// Presents are ordered: a buffer whose fence signals after a later present
// already landed is dropped, so the view always ends on the last buffer
// presented.
//
// An entry has two owners: the JS component (retainFromJS on mount,
// releaseFromJS on unmount, releaseAllFromJS on reload) and the native view
// (setListener / detachView). It is erased once both let go, in either order.
// A native view that attaches after the JS component let go still gets an
// entry, which detachView erases. React StrictMode's simulated unmount (JS
// effects re-run, native view kept) releases and retains again.
class NativeBufferViewRegistry {
public:
  using Listener = std::function<void()>;

  static NativeBufferViewRegistry &getInstance() {
    static NativeBufferViewRegistry instance;
    return instance;
  }

  NativeBufferViewRegistry(const NativeBufferViewRegistry &) = delete;
  NativeBufferViewRegistry &
  operator=(const NativeBufferViewRegistry &) = delete;

  // Unique for the life of the process: the registry outlives a JS reload.
  // Creates no entry (React StrictMode may discard the id): retainFromJS does.
  int allocateViewId() {
    std::lock_guard<std::mutex> lock(_mutex);
    return ++_nextViewId;
  }

  // Makes `content` the view's content once `fence` (if any) has reached
  // `signaledValue` and, on Android, once `acquireFenceFd` (a sync fd the
  // producer signals when it is done writing the buffer, or -1) has
  // signaled. Takes ownership of `acquireFenceFd`. Returns immediately.
  //
  // A present to a view the JS component does not hold (released, or never
  // retained) is dropped: nothing would ever free its buffer.
  void present(int viewId, std::shared_ptr<NativeBufferContent> content,
               wgpu::SharedFence fence, uint64_t signaledValue,
               int acquireFenceFd = -1);

  // The latest ready content and its sequence number (0 when there is none
  // yet). A view keeps the sequence number of what it shows and skips a
  // content it already has.
  std::shared_ptr<NativeBufferContent> content(int viewId, uint64_t *sequence);

  // The native view attaches (a listener) or detaches (nullptr). The listener
  // is called from any thread whenever new content is ready, and once right
  // away if the view already has content.
  void setListener(int viewId, Listener listener);

  // The native view is gone for good: drops its listener, and the entry when
  // the JS component has released it too.
  void detachView(int viewId);

  // The JS component mounted (or mounted again with the same id, when React
  // StrictMode re-runs its effects): creates the entry if needed and takes
  // the JS ownership.
  void retainFromJS(int viewId);

  // The JS component unmounted: drops the entry when no native view is
  // attached to it (else detachView does).
  void releaseFromJS(int viewId);

  // The JS runtime is going away (a reload): its components never run their
  // cleanup, so this releases every entry from JS.
  void releaseAllFromJS();

private:
  NativeBufferViewRegistry() = default;

  struct Entry {
    std::shared_ptr<NativeBufferContent> content;
    // The last present, and the one `content` comes from.
    uint64_t presentedSequence = 0;
    uint64_t readySequence = 0;
    Listener listener;
    bool ownedByJS = false;
  };

  // Erases `it` when neither the JS component nor a native view holds it,
  // moving its content to `released` so the buffer is freed outside the lock.
  void eraseIfUnowned(std::unordered_map<int, Entry>::iterator it,
                      std::shared_ptr<NativeBufferContent> *released);

  void deliver(int viewId, uint64_t sequence,
               std::shared_ptr<NativeBufferContent> content);

  std::mutex _mutex;
  std::unordered_map<int, Entry> _entries;
  int _nextViewId = 0;
};

} // namespace rnwgpu
