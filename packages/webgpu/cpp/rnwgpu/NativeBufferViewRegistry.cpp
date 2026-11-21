#include "NativeBufferViewRegistry.h"

#include <cerrno>
#include <condition_variable>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__ANDROID__)
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <poll.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <IOSurface/IOSurfaceRef.h>
#endif

namespace rnwgpu {

namespace {

constexpr const char *kCaller = "presentNativeBuffer";

[[noreturn]] void fail(const std::string &message) {
  throw std::runtime_error(std::string(kCaller) + "(): " + message);
}

} // namespace

// --- NativeBufferContent ----------------------------------------------------

std::shared_ptr<NativeBufferContent>
NativeBufferContent::fromFrame(const VideoFrameHandle &frame) {
  if (frame.handle == nullptr) {
    fail("the frame has been released");
  }
  if (frame.pixelFormat == VideoPixelFormat::NV12) {
    fail("a YUV frame can't be displayed as it is; draw it into an RGBA "
         "buffer (createNativeBuffer) first");
  }
#if defined(__ANDROID__)
  AHardwareBuffer_Desc desc{};
  AHardwareBuffer_describe(static_cast<AHardwareBuffer *>(frame.handle), &desc);
  switch (desc.format) {
  case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
  case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
  case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
    break;
  default:
    fail("unsupported AHardwareBuffer format " + std::to_string(desc.format) +
         " (expected R8G8B8A8_UNORM, R8G8B8X8_UNORM or R16G16B16A16_FLOAT)");
  }
  if (desc.layers != 1) {
    fail("the AHardwareBuffer has " + std::to_string(desc.layers) +
         " layers; only single-layer buffers can be displayed");
  }
#elif defined(__APPLE__)
  const OSType format =
      IOSurfaceGetPixelFormat(static_cast<IOSurfaceRef>(frame.handle));
  if (format != kCVPixelFormatType_32BGRA &&
      format != kCVPixelFormatType_64RGBAHalf) {
    fail("unsupported IOSurface pixel format (expected 'BGRA' or 'RGhA', "
         "that is bgra8unorm or rgba16float)");
  }
#endif
  const auto colorSpace =
      frame.colorSpace.primaries == wgpu::ColorSpacePrimariesDawn::DisplayP3
          ? NativeBufferColorSpace::DisplayP3
          : NativeBufferColorSpace::SRGB;
  return std::shared_ptr<NativeBufferContent>(new NativeBufferContent(
      frame.handle, frame.width, frame.height, colorSpace));
}

NativeBufferContent::NativeBufferContent(void *buffer, uint32_t width,
                                         uint32_t height,
                                         NativeBufferColorSpace colorSpace)
    : _buffer(buffer), _width(width), _height(height), _colorSpace(colorSpace) {
#if defined(__ANDROID__)
  AHardwareBuffer_acquire(static_cast<AHardwareBuffer *>(_buffer));
#elif defined(__APPLE__)
  CFRetain(_buffer);
#endif
}

NativeBufferContent::~NativeBufferContent() {
#if defined(__ANDROID__)
  AHardwareBuffer_release(static_cast<AHardwareBuffer *>(_buffer));
#elif defined(__APPLE__)
  CFRelease(_buffer);
#endif
}

// --- Fence wait (Android; Apple platforms: NativeBufferFenceWait.mm) ---------

#if defined(__ANDROID__)

namespace {

#define NBV_LOG_TAG "WebGPUNativeBufferView"

// One thread for every NativeBufferView, started on first use and kept for
// the life of the process. Presents are occasional, and their fences signal
// in submission order, so a single FIFO is enough.
class FenceWaiter {
public:
  static FenceWaiter &shared() {
    // Leaked on purpose: the detached thread may still be waiting at exit.
    static FenceWaiter *waiter = new FenceWaiter();
    return *waiter;
  }

  // Waits for `fenceFd` (borrowed from `fence`, -1 for none) and `ownedFd`
  // (closed after the wait, -1 for none).
  void enqueue(wgpu::SharedFence fence, int fenceFd, int ownedFd,
               std::function<void(bool)> done) {
    std::lock_guard<std::mutex> lock(_mutex);
    _jobs.push_back({std::move(fence), fenceFd, ownedFd, std::move(done)});
    if (!_started) {
      _started = true;
      std::thread([this]() { run(); }).detach();
    }
    _cv.notify_one();
  }

private:
  struct Job {
    // Owns `fenceFd`: Dawn's export is borrowed and closed with the fence.
    wgpu::SharedFence fence;
    int fenceFd;
    int ownedFd;
    std::function<void(bool)> done;
  };

  // A fence that has not signaled after this long belongs to work that will
  // never finish (a lost device): the buffer is dropped.
  static constexpr int kTimeoutMs = 10000;

  void run() {
    for (;;) {
      Job job;
      {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this]() { return !_jobs.empty(); });
        job = std::move(_jobs.front());
        _jobs.pop_front();
      }
      bool signaled = job.fenceFd < 0 || waitFd(job.fenceFd);
      if (job.ownedFd >= 0) {
        signaled = signaled && waitFd(job.ownedFd);
        ::close(job.ownedFd);
      }
      if (!signaled) {
        __android_log_print(ANDROID_LOG_WARN, NBV_LOG_TAG,
                            "the fence of a presented buffer did not signal; "
                            "the buffer is dropped");
      }
      job.done(signaled);
      // The fence (and its fd) is released here, after the wait.
    }
  }

  // Sync fds turn readable (POLLIN) once signaled. poll() keeps this to libc,
  // with no libsync dependency.
  static bool waitFd(int fd) {
    struct pollfd pfd = {fd, POLLIN, 0};
    for (;;) {
      const int result = ::poll(&pfd, 1, kTimeoutMs);
      if (result > 0) {
        return (pfd.revents & (POLLERR | POLLNVAL)) == 0;
      }
      if (result < 0 && errno == EINTR) {
        continue;
      }
      return false;
    }
  }

  std::mutex _mutex;
  std::condition_variable _cv;
  std::deque<Job> _jobs;
  bool _started = false;
};

} // namespace

namespace {

// Waits for `fence` (if any) and `ownedFd` (if >= 0, closed afterwards).
void waitForFences(wgpu::SharedFence fence, int ownedFd,
                   std::function<void(bool)> done) {
  int fenceFd = -1;
  if (fence) {
    // Exported on the calling thread, where the device is in use anyway.
    wgpu::SharedFenceExportInfo info{};
    wgpu::SharedFenceSyncFDExportInfo fdInfo{};
    info.nextInChain = &fdInfo;
    fence.ExportInfo(&info);
    if (info.type != wgpu::SharedFenceType::SyncFD) {
      __android_log_print(ANDROID_LOG_WARN, NBV_LOG_TAG,
                          "presentNativeBuffer: the fence is not a sync fd; "
                          "the buffer is dropped");
      if (ownedFd >= 0) {
        ::close(ownedFd);
      }
      done(false);
      return;
    }
    // -1 is a fence that has already signaled.
    fenceFd = fdInfo.handle;
  }
  if (fenceFd < 0 && ownedFd < 0) {
    done(true);
    return;
  }
  FenceWaiter::shared().enqueue(std::move(fence), fenceFd, ownedFd,
                                std::move(done));
}

} // namespace

void waitForSharedFence(wgpu::SharedFence fence, uint64_t /* signaledValue */,
                        std::function<void(bool)> done) {
  waitForFences(std::move(fence), -1, std::move(done));
}

#endif // __ANDROID__

// --- NativeBufferViewRegistry ------------------------------------------------

void NativeBufferViewRegistry::present(
    int viewId, std::shared_ptr<NativeBufferContent> content,
    wgpu::SharedFence fence, uint64_t signaledValue, int acquireFenceFd) {
  uint64_t sequence = 0;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(viewId);
    if (it != _entries.end() && it->second.ownedByJS) {
      sequence = ++it->second.presentedSequence;
    }
  }
  if (sequence == 0) {
    // Released by JS (a present that lands after unmount), or an id that
    // was never retained.
#if defined(__ANDROID__)
    if (acquireFenceFd >= 0) {
      ::close(acquireFenceFd);
    }
#endif
    return;
  }
  auto onSignaled = [viewId, sequence,
                     content = std::move(content)](bool signaled) mutable {
    if (signaled) {
      NativeBufferViewRegistry::getInstance().deliver(viewId, sequence,
                                                      std::move(content));
    }
  };
#if defined(__ANDROID__)
  // Sync fds have no value to wait for.
  (void)signaledValue;
  if (fence || acquireFenceFd >= 0) {
    waitForFences(std::move(fence), acquireFenceFd, std::move(onSignaled));
    return;
  }
#else
  (void)acquireFenceFd;
  if (fence) {
    waitForSharedFence(std::move(fence), signaledValue, std::move(onSignaled));
    return;
  }
#endif
  onSignaled(true);
}

void NativeBufferViewRegistry::deliver(
    int viewId, uint64_t sequence,
    std::shared_ptr<NativeBufferContent> content) {
  Listener listener;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(viewId);
    // Gone (the view unmounted while the fence was pending), or superseded
    // by a later present whose fence signaled first.
    if (it == _entries.end() || sequence <= it->second.readySequence) {
      return;
    }
    it->second.readySequence = sequence;
    it->second.content = std::move(content);
    listener = it->second.listener;
  }
  // Outside the lock: the listener may read the content back.
  if (listener) {
    listener();
  }
}

std::shared_ptr<NativeBufferContent>
NativeBufferViewRegistry::content(int viewId, uint64_t *sequence) {
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _entries.find(viewId);
  if (it == _entries.end()) {
    *sequence = 0;
    return nullptr;
  }
  *sequence = it->second.readySequence;
  return it->second.content;
}

void NativeBufferViewRegistry::setListener(int viewId, Listener listener) {
  if (viewId <= 0) {
    return;
  }
  bool hasContent = false;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    // Created when the JS component already let go: detachView erases it.
    auto &entry = _entries[viewId];
    entry.listener = listener;
    hasContent = entry.content != nullptr;
  }
  if (listener && hasContent) {
    listener();
  }
}

void NativeBufferViewRegistry::detachView(int viewId) {
  std::shared_ptr<NativeBufferContent> released;
  Listener listener;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(viewId);
    if (it == _entries.end()) {
      return;
    }
    listener = std::move(it->second.listener);
    it->second.listener = nullptr;
    eraseIfUnowned(it, &released);
  }
  // The buffer and the listener's captures are released outside the lock.
}

void NativeBufferViewRegistry::retainFromJS(int viewId) {
  if (viewId <= 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(_mutex);
  _entries[viewId].ownedByJS = true;
}

void NativeBufferViewRegistry::releaseFromJS(int viewId) {
  std::shared_ptr<NativeBufferContent> released;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(viewId);
    if (it == _entries.end()) {
      return;
    }
    it->second.ownedByJS = false;
    eraseIfUnowned(it, &released);
  }
}

void NativeBufferViewRegistry::releaseAllFromJS() {
  std::vector<std::shared_ptr<NativeBufferContent>> released;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto it = _entries.begin(); it != _entries.end();) {
      it->second.ownedByJS = false;
      if (it->second.listener) {
        ++it;
      } else {
        released.push_back(std::move(it->second.content));
        it = _entries.erase(it);
      }
    }
  }
}

void NativeBufferViewRegistry::eraseIfUnowned(
    std::unordered_map<int, Entry>::iterator it,
    std::shared_ptr<NativeBufferContent> *released) {
  if (it->second.ownedByJS || it->second.listener) {
    return;
  }
  *released = std::move(it->second.content);
  _entries.erase(it);
}

} // namespace rnwgpu
