#pragma once

#include <android/choreographer.h>
#include <android/log.h>
#include <android/looper.h>
#include <dlfcn.h>
#include <pthread.h>

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>

#include "AnimationFrames.h"

namespace rnwgpu {

/**
 * The display's frames on Android: an AChoreographer on a looper thread of its
 * own, so that a busy main thread or JS thread never delays them. A request
 * from any thread sets a flag and wakes the looper, which posts one frame
 * callback; the frame time is the choreographer's, on CLOCK_MONOTONIC (the
 * clock of std::chrono::steady_clock). The thread lives as long as the
 * process.
 */
class AndroidAnimationFrameSource : public AnimationFrameSource {
public:
  // The process's source, made on first use (its thread then runs for the
  // life of the process); null when the thread gets no choreographer. A
  // reload asks again and gets the same one.
  static std::shared_ptr<AndroidAnimationFrameSource> shared() {
    static std::shared_ptr<AndroidAnimationFrameSource> source = create();
    return source;
  }

  void requestFrame() override {
    _wanted.store(true, std::memory_order_release);
    ALooper_wake(_looper);
  }

private:
  AndroidAnimationFrameSource() = default;

  static std::shared_ptr<AndroidAnimationFrameSource> create() {
    auto source = std::shared_ptr<AndroidAnimationFrameSource>(
        new AndroidAnimationFrameSource());
    // Shared with the thread, which may still be inside set_value() when
    // get() returns here.
    auto ready = std::make_shared<std::promise<ALooper *>>();
    auto looperFuture = ready->get_future();
    // The thread borrows the source, which shared() keeps for the life of the
    // process (when the thread fails, it returns before this returns).
    auto *raw = source.get();
    std::thread([raw, ready]() { raw->run(*ready); }).detach();
    source->_looper = looperFuture.get();
    if (source->_looper == nullptr) {
      return nullptr;
    }
    return source;
  }

  using FrameCallback64 = void (*)(int64_t, void *);
  using PostFrameCallback64 = void (*)(AChoreographer *, FrameCallback64,
                                       void *);

  void run(std::promise<ALooper *> &ready) {
    pthread_setname_np(pthread_self(), "RNWebGPU Frames");
    ALooper *looper = ALooper_prepare(0);
    _choreographer = AChoreographer_getInstance();
    if (looper == nullptr || _choreographer == nullptr) {
      __android_log_print(ANDROID_LOG_WARN, "RNWebGPU",
                          "No choreographer on the animation frame thread: "
                          "requestAnimationFrame falls back to the runtime's.");
      ready.set_value(nullptr);
      return;
    }
    ALooper_acquire(looper);
    // API 29; the library builds for API 26.
    _postFrameCallback64 = reinterpret_cast<PostFrameCallback64>(
        dlsym(RTLD_DEFAULT, "AChoreographer_postFrameCallback64"));
    ready.set_value(looper);
    for (;;) {
      ALooper_pollOnce(-1, nullptr, nullptr, nullptr);
      if (_wanted.exchange(false, std::memory_order_acq_rel) && !_posted) {
        post();
      }
    }
  }

  // Looper thread.
  void post() {
    _posted = true;
    if (_postFrameCallback64 != nullptr) {
      _postFrameCallback64(
          _choreographer,
          [](int64_t frameTimeNanos, void *data) {
            static_cast<AndroidAnimationFrameSource *>(data)->onFrame(
                frameTimeNanos);
          },
          this);
      return;
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // The entry point of API 24: its frame time is a long, 32 bits on a
    // 32-bit device, so the clock is read instead there.
    AChoreographer_postFrameCallback(
        _choreographer,
        [](long frameTimeNanos, void *data) {
          static_cast<AndroidAnimationFrameSource *>(data)->onFrame(
              sizeof(long) >= sizeof(int64_t)
                  ? static_cast<int64_t>(frameTimeNanos)
                  : AnimationFrameDispatcher::nowNanos());
        },
        this);
#pragma clang diagnostic pop
  }

  // Looper thread.
  void onFrame(int64_t frameTimeNanos) {
    _posted = false;
    deliverFrame(frameTimeNanos);
    // A request made while the frame was delivered (on this thread, or
    // another) is in _wanted, and its wake is pending: the loop posts it.
  }

  ALooper *_looper = nullptr;
  AChoreographer *_choreographer = nullptr;
  PostFrameCallback64 _postFrameCallback64 = nullptr;
  std::atomic<bool> _wanted{false};
  // Looper thread only.
  bool _posted = false;
};

} // namespace rnwgpu
