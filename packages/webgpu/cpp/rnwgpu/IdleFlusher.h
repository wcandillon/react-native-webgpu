#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace rnwgpu {

// Runs `flush` once things have been quiet for a while, for work that is worth
// persisting but not worth doing on every change. Dawn's monolithic Vulkan
// pipeline cache is one: it only reaches the blob cache when the embedder calls
// PerformIdleTasks() on the device, and each store rewrites the whole cache.
//
// touch() records that there is something to flush. In the background mode a
// thread, started by the first touch(), flushes `quiet` after the last touch(),
// or `maxDelay` after the first one since the last flush if touches keep
// coming. Otherwise nothing runs by itself, and flushNow() and finish() are the
// only ways to flush.
//
// A flush never overlaps another one. The thread owns the state it works on,
// so destroying the IdleFlusher never waits for it: that makes the destructor
// safe to run wherever the last reference to its owner is dropped, a Dawn
// callback holding a device lock included. Call finish() or stop() first where
// the flush must be over, or what it uses released. A flush still running when
// the process exits is not waited for either, so what it touches must outlive
// the process's static destructors.
class IdleFlusher {
public:
  using Clock = std::chrono::steady_clock;

  IdleFlusher(std::function<void()> flush, std::chrono::milliseconds quiet,
              std::chrono::milliseconds maxDelay, bool background);
  // Cancels, without waiting.
  ~IdleFlusher();

  IdleFlusher(const IdleFlusher &) = delete;
  IdleFlusher &operator=(const IdleFlusher &) = delete;

  // Thread-safe and cheap. Does nothing once cancelled.
  void touch();

  // Flushes on the calling thread if anything was touched since the last flush.
  void flushNow();

  // No flush starts after this, and `flush` is released as soon as none is
  // running (by whichever thread is last to use it). In a narrow race with a
  // flush that is ending it can be kept until stop() or finish(), or until
  // the IdleFlusher is destroyed. Does not wait, so it can be called from a
  // callback that may run inside a flush.
  void cancel();

  // Flushes what was touched and not flushed yet, on the calling thread, then
  // cancels, as one step: nothing touched before the call is dropped, and no
  // flush starts or is running when it returns. Safe to call from several
  // threads. Must not be called from `flush` itself.
  void finish();

  // cancel(), then waits for a flush in progress. Same rules as finish().
  void stop();

  // Whether the calling thread is inside a flush. A callback that must not
  // reach JavaScript from a thread of Dawn's (a logging callback) can use this
  // to tell a flush's messages from the ones it normally gets.
  static bool flushing();

private:
  struct State {
    State(std::function<void()> flushFunction, std::chrono::milliseconds quiet,
          std::chrono::milliseconds maxDelay, bool background)
        : quiet(quiet), maxDelay(maxDelay), background(background),
          flush(std::move(flushFunction)) {}

    Clock::time_point due() const;
    void run();
    void runFlush();
    // Drops `flush` if cancelled; the caller holds flushMutex.
    void releaseIfCancelled();

    const std::chrono::milliseconds quiet;
    const std::chrono::milliseconds maxDelay;
    const bool background;

    // Guards everything below it up to flushMutex. Never held while waiting
    // for flushMutex.
    std::mutex mutex;
    std::condition_variable wake;
    bool dirty = false;
    bool cancelled = false;
    bool threadStarted = false;
    Clock::time_point firstTouch;
    Clock::time_point lastTouch;

    // Serializes flushes and guards `flush`. Taken before `mutex`, never after.
    std::mutex flushMutex;
    std::function<void()> flush;
  };

  const std::shared_ptr<State> _state;
};

} // namespace rnwgpu
