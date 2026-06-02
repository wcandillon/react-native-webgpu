#pragma once

#include <functional>

#include <jsi/jsi.h>

namespace rnwgpu::async {

namespace jsi = facebook::jsi;

/**
 * Thread-safe "post this job onto a specific runtime's JS thread".
 *
 * Replaces the old AsyncDispatcher / JSIMicrotaskDispatcher, whose
 * queueMicrotask-based dispatch was only safe to call from the runtime's own
 * thread. A RuntimeScheduler can be called from any thread (e.g. the
 * GpuEventLoop background threads) and guarantees the job runs on the owning
 * runtime's JS thread.
 */
class RuntimeScheduler {
public:
  virtual ~RuntimeScheduler() = default;

  /**
   * Schedule `job` to run on this runtime's JS thread. Callable from any
   * thread. Jobs are delivered in FIFO order relative to one another.
   */
  virtual void scheduleOnJS(std::function<void(jsi::Runtime &)> job) = 0;
};

} // namespace rnwgpu::async
