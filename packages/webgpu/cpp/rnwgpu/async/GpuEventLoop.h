#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu::async {

/**
 * Background, event-driven driver for Dawn async operations. Replaces the old
 * JS-thread ProcessEvents polling loop.
 *
 * Each pending wgpu::Future (registered with CallbackMode::WaitAnyOnly) is
 * handed to addFuture() and waited on by a worker thread via
 * `instance.WaitAny(future, UINT64_MAX)`. The wait is genuinely event-driven
 * (zero idle CPU) and resolves the instant the GPU work completes, at which
 * point Dawn fires the future's callback on the worker thread. That callback is
 * responsible for marshalling back to the owning runtime's JS thread (via a
 * RuntimeScheduler) to settle the JS Promise.
 *
 * Threading model (validated in Phase 0, spike 2): each WaitAny call waits on a
 * *single* future, which is always a legal single-source wait. Multiple workers
 * may block in WaitAny on the same instance concurrently; Dawn's EventManager
 * is designed for this.
 *
 * The worker pool grows lazily up to a small cap as concurrent work demands,
 * and threads are reused. Shared state is held behind a shared_ptr so detached
 * workers (and the wgpu::Instance ref they need) outlive this object safely.
 */
class GpuEventLoop {
public:
  explicit GpuEventLoop(wgpu::Instance instance);
  ~GpuEventLoop();

  GpuEventLoop(const GpuEventLoop &) = delete;
  GpuEventLoop &operator=(const GpuEventLoop &) = delete;

  /**
   * Wait for `future` to complete on a background thread. A future with id == 0
   * (no event to wait on, e.g. a deferred/immediate resolution) is ignored.
   * Thread-safe.
   */
  void addFuture(wgpu::Future future);

private:
  struct State {
    explicit State(wgpu::Instance instance) : instance(std::move(instance)) {}

    wgpu::Instance instance;
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<wgpu::Future> queue;
    std::atomic_bool running{true};
    std::size_t idleWorkers = 0;
    std::size_t totalWorkers = 0;
    std::size_t maxWorkers = 1;
  };

  static void worker(std::shared_ptr<State> state);

  std::shared_ptr<State> _state;
};

} // namespace rnwgpu::async
