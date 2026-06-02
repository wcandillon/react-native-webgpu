#pragma once

#include <functional>
#include <memory>

#include <jsi/jsi.h>

#include "AsyncTaskHandle.h"
#include "GpuEventLoop.h"
#include "RuntimeScheduler.h"

#include "webgpu/webgpu_cpp.h"

namespace jsi = facebook::jsi;

namespace rnwgpu::async {

/**
 * Per-runtime coordinator for asynchronous WebGPU operations.
 *
 * Bundles the runtime's RuntimeScheduler (how to settle Promises back on the
 * owning JS thread) with the GpuEventLoop (how to wait on Dawn futures off the
 * JS thread). This replaces the previous ProcessEvents polling design: there is
 * no tick loop and no idle CPU usage.
 *
 * A task callback registers a Dawn async op with CallbackMode::WaitAnyOnly and
 * returns the resulting wgpu::Future, which is handed to the GpuEventLoop. A
 * returned future with id == 0 means "no event to wait on" (deferred/immediate
 * resolution, e.g. GPUDevice::getLost).
 */
class RuntimeContext : public std::enable_shared_from_this<RuntimeContext> {
public:
  using TaskCallback =
      std::function<wgpu::Future(const AsyncTaskHandle::ResolveFunction &,
                                 const AsyncTaskHandle::RejectFunction &)>;

  RuntimeContext(std::shared_ptr<RuntimeScheduler> scheduler,
                 std::shared_ptr<GpuEventLoop> eventLoop);

  static std::shared_ptr<RuntimeContext> get(jsi::Runtime &runtime);
  static std::shared_ptr<RuntimeContext>
  getOrCreate(jsi::Runtime &runtime,
              std::shared_ptr<RuntimeScheduler> scheduler,
              std::shared_ptr<GpuEventLoop> eventLoop);

  AsyncTaskHandle postTask(const TaskCallback &callback);

  std::shared_ptr<RuntimeScheduler> scheduler() const;

private:
  static jsi::UUID runtimeDataUUID();

  std::shared_ptr<RuntimeScheduler> _scheduler;
  std::shared_ptr<GpuEventLoop> _eventLoop;
};

} // namespace rnwgpu::async
