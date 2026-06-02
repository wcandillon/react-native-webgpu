#include "RuntimeContext.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "AsyncTaskHandle.h"
#include "WGPULogger.h"

namespace rnwgpu::async {

namespace {
struct RuntimeData {
  std::shared_ptr<RuntimeContext> runner;
};
constexpr const char *TAG = "RuntimeContext";
} // namespace

RuntimeContext::RuntimeContext(std::shared_ptr<RuntimeScheduler> scheduler,
                               std::shared_ptr<GpuEventLoop> eventLoop)
    : _scheduler(std::move(scheduler)), _eventLoop(std::move(eventLoop)) {
  if (!_scheduler) {
    throw std::runtime_error(
        "RuntimeContext requires a valid RuntimeScheduler.");
  }
  if (!_eventLoop) {
    throw std::runtime_error("RuntimeContext requires a valid GpuEventLoop.");
  }
  Logger::logToConsole("[%s] Created runner (scheduler=%p, eventLoop=%p)", TAG,
                       _scheduler.get(), _eventLoop.get());
}

std::shared_ptr<RuntimeContext> RuntimeContext::get(jsi::Runtime &runtime) {
  auto data = runtime.getRuntimeData(runtimeDataUUID());
  if (!data) {
    return nullptr;
  }
  auto stored = std::static_pointer_cast<RuntimeData>(data);
  return stored->runner;
}

std::shared_ptr<RuntimeContext>
RuntimeContext::getOrCreate(jsi::Runtime &runtime,
                            std::shared_ptr<RuntimeScheduler> scheduler,
                            std::shared_ptr<GpuEventLoop> eventLoop) {
  auto existing = get(runtime);
  if (existing) {
    return existing;
  }

  auto runner = std::make_shared<RuntimeContext>(std::move(scheduler),
                                                 std::move(eventLoop));
  auto data = std::make_shared<RuntimeData>();
  data->runner = runner;
  runtime.setRuntimeData(runtimeDataUUID(), data);
  return runner;
}

AsyncTaskHandle RuntimeContext::postTask(const TaskCallback &callback) {
  auto handle = AsyncTaskHandle::create(_scheduler);
  if (!handle.valid()) {
    throw std::runtime_error("Failed to create AsyncTaskHandle.");
  }

  auto resolve = handle.createResolveFunction();
  auto reject = handle.createRejectFunction();

  wgpu::Future future{};
  try {
    future = callback(resolve, reject);
  } catch (const std::exception &exception) {
    reject(exception.what());
    return handle;
  } catch (...) {
    reject("Unknown native error in RuntimeContext::postTask.");
    return handle;
  }

  _eventLoop->addFuture(future);
  return handle;
}

std::shared_ptr<RuntimeScheduler> RuntimeContext::scheduler() const {
  return _scheduler;
}

jsi::UUID RuntimeContext::runtimeDataUUID() {
  static const auto uuid = jsi::UUID();
  return uuid;
}

} // namespace rnwgpu::async
