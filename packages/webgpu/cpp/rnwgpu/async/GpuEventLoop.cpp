#include "GpuEventLoop.h"

#include <algorithm>
#include <cstdint>
#include <thread>
#include <utility>

#include "WGPULogger.h"

namespace rnwgpu::async {

namespace {
constexpr const char *TAG = "GpuEventLoop";

std::size_t computeMaxWorkers() {
  unsigned int hw = std::thread::hardware_concurrency();
  if (hw == 0) {
    hw = 4;
  }
  // A small bounded pool: enough to overlap the handful of async GPU ops that
  // are realistically in flight at once, without spawning unbounded threads.
  return std::max<std::size_t>(2, std::min<std::size_t>(8, hw));
}
} // namespace

GpuEventLoop::GpuEventLoop(wgpu::Instance instance)
    : _state(std::make_shared<State>(std::move(instance))) {
  _state->maxWorkers = computeMaxWorkers();
  Logger::logToConsole("[%s] Created (maxWorkers=%zu)", TAG,
                       _state->maxWorkers);
}

GpuEventLoop::~GpuEventLoop() {
  {
    std::lock_guard<std::mutex> lock(_state->mutex);
    _state->running.store(false, std::memory_order_release);
  }
  // Wake idle workers so they can observe !running and exit. Workers that are
  // currently blocked in WaitAny keep the shared State (and its wgpu::Instance
  // ref) alive until their future completes, then exit; we intentionally do not
  // join here to avoid blocking teardown on in-flight GPU work.
  _state->cv.notify_all();
}

void GpuEventLoop::addFuture(wgpu::Future future) {
  if (future.id == 0) {
    // No event to wait on (deferred/immediate resolution). The callback path
    // settles the promise without involving the event loop.
    return;
  }

  std::lock_guard<std::mutex> lock(_state->mutex);
  if (!_state->running.load(std::memory_order_acquire)) {
    return;
  }

  _state->queue.push(future);

  // Grow the pool if every worker is busy and we are still under the cap;
  // otherwise wake an idle worker. A freshly spawned worker picks the job up
  // via the queue-non-empty predicate, so it needs no separate notify.
  if (_state->idleWorkers == 0 && _state->totalWorkers < _state->maxWorkers) {
    _state->totalWorkers++;
    std::thread(&GpuEventLoop::worker, _state).detach();
    Logger::logToConsole("[%s] grew pool to %zu worker(s)", TAG,
                         _state->totalWorkers);
  } else {
    _state->cv.notify_one();
  }
}

void GpuEventLoop::worker(std::shared_ptr<State> state) {
  for (;;) {
    wgpu::Future future{};
    {
      std::unique_lock<std::mutex> lock(state->mutex);
      state->idleWorkers++;
      state->cv.wait(lock, [&state] {
        return !state->running.load(std::memory_order_acquire) ||
               !state->queue.empty();
      });
      state->idleWorkers--;

      if (state->queue.empty()) {
        // Only happens when shutting down.
        state->totalWorkers--;
        return;
      }

      future = state->queue.front();
      state->queue.pop();
    }

    // Single-future wait: always a legal single-source WaitAny. Blocks with no
    // CPU cost until the GPU work completes, at which point Dawn invokes the
    // future's callback on this thread (it then marshals back to the owning
    // runtime via its RuntimeScheduler).
    auto status = state->instance.WaitAny(future, UINT64_MAX);
    if (status != wgpu::WaitStatus::Success) {
      // With an infinite timeout on a single future this is not expected. If it
      // happens, Dawn did not invoke the future's callback, so the associated
      // JS Promise will never settle. Log it so the otherwise-silent hang is at
      // least observable.
      Logger::logToConsole(
          "[%s] WaitAny returned non-success status %u for future %llu; its "
          "Promise will not settle.",
          TAG, static_cast<unsigned int>(status),
          static_cast<unsigned long long>(future.id));
    }
  }
}

} // namespace rnwgpu::async
