#include "AsyncRunner.h"

#include <stdexcept>
#include <utility>

#include "AsyncTaskHandle.h"
#include "WGPULogger.h"

namespace rnwgpu::async {

namespace {
struct RuntimeData {
  std::shared_ptr<AsyncRunner> runner;
};
constexpr const char *TAG = "AsyncRunner";
} // namespace

AsyncRunner::AsyncRunner(wgpu::Instance instance, std::shared_ptr<AsyncDispatcher> dispatcher)
    : _instance(std::move(instance)), _dispatcher(std::move(dispatcher)), _pendingTasks(0), _pumpTasks(0),
      _tickScheduled(false) {
  if (!_dispatcher) {
    throw std::runtime_error("AsyncRunner requires a valid dispatcher.");
  }
  Logger::logToConsole("[%s] Created runner (dispatcher=%p)", TAG, _dispatcher.get());
}

std::shared_ptr<AsyncRunner> AsyncRunner::get(jsi::Runtime& runtime) {
  auto data = runtime.getRuntimeData(runtimeDataUUID());
  if (!data) {
    return nullptr;
  }
  auto stored = std::static_pointer_cast<RuntimeData>(data);
  return stored->runner;
}

std::shared_ptr<AsyncRunner> AsyncRunner::getOrCreate(jsi::Runtime& runtime, wgpu::Instance instance,
                                                      std::shared_ptr<AsyncDispatcher> dispatcher) {
  auto existing = get(runtime);
  if (existing) {
    return existing;
  }

  auto runner = std::make_shared<AsyncRunner>(std::move(instance), std::move(dispatcher));
  auto data = std::make_shared<RuntimeData>();
  data->runner = runner;
  runtime.setRuntimeData(runtimeDataUUID(), data);
  return runner;
}

AsyncTaskHandle AsyncRunner::postTask(const TaskCallback& callback, bool keepPumping) {
  auto handle = AsyncTaskHandle::create(shared_from_this(), keepPumping);
  if (!handle.valid()) {
    throw std::runtime_error("Failed to create AsyncTaskHandle.");
  }

  _pendingTasks.fetch_add(1, std::memory_order_acq_rel);
  if (keepPumping) {
    _pumpTasks.fetch_add(1, std::memory_order_acq_rel);
  }
  requestTick();

  Logger::logToConsole("[%s] postTask (keepPumping=%s, pending=%zu, pumping=%zu)", TAG,
                       keepPumping ? "true" : "false",
                       _pendingTasks.load(std::memory_order_acquire),
                       _pumpTasks.load(std::memory_order_acquire));

  auto resolve = handle.createResolveFunction();
  auto reject = handle.createRejectFunction();

  try {
    callback(resolve, reject);
  } catch (const std::exception& exception) {
    reject(exception.what());
  } catch (...) {
    reject("Unknown native error in AsyncRunner::postTask.");
  }

  return handle;
}

void AsyncRunner::requestTick() {
  bool expected = false;
  if (!_tickScheduled.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    Logger::logToConsole("[%s] requestTick skipped (already scheduled)", TAG);
    return;
  }

  auto self = shared_from_this();
  Logger::logToConsole("[%s] requestTick scheduled microtask", TAG);
  _dispatcher->post([self](jsi::Runtime& runtime) {
    self->tick(runtime);
  });
}

void AsyncRunner::tick(jsi::Runtime& /*runtime*/) {
  _tickScheduled.store(false, std::memory_order_release);
  _instance.ProcessEvents();
  Logger::logToConsole("[%s] tick processed events (pending=%zu, pumping=%zu)", TAG,
                       _pendingTasks.load(std::memory_order_acquire),
                       _pumpTasks.load(std::memory_order_acquire));
  if (_pumpTasks.load(std::memory_order_acquire) > 0) {
    requestTick();
  }
}

void AsyncRunner::onTaskSettled(bool keepPumping) {
  _pendingTasks.fetch_sub(1, std::memory_order_acq_rel);
  if (keepPumping) {
    _pumpTasks.fetch_sub(1, std::memory_order_acq_rel);
  }
  Logger::logToConsole("[%s] onTaskSettled (keepPumping=%s, pending=%zu, pumping=%zu)", TAG,
                       keepPumping ? "true" : "false",
                       _pendingTasks.load(std::memory_order_acquire),
                       _pumpTasks.load(std::memory_order_acquire));
}

std::shared_ptr<AsyncDispatcher> AsyncRunner::dispatcher() const {
  return _dispatcher;
}

jsi::UUID AsyncRunner::runtimeDataUUID() {
  static const auto uuid = jsi::UUID();
  return uuid;
}

} // namespace rnwgpu::async
