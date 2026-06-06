#include "RuntimeContext.h"

#include <memory>
#include <stdexcept>
#include <utility>

#include "AsyncTaskHandle.h"
#include "WGPULogger.h"

namespace rnwgpu::async {

namespace {
struct RuntimeData {
  std::shared_ptr<RuntimeContext> context;
};
constexpr const char *TAG = "RuntimeContext";

// Serializes ProcessEvents() across all runtimes that share a wgpu::Instance.
// Held only across the ProcessEvents call itself, never while running JS / mailbox
// settle-actions, so it cannot deadlock against the per-context mailbox mutex.
std::mutex &processEventsMutex() {
  static std::mutex mutex;
  return mutex;
}
} // namespace

RuntimeContext::RuntimeContext(jsi::Runtime &runtime, wgpu::Instance instance)
    : _runtime(runtime), _instance(std::move(instance)) {
  Logger::logToConsole("[%s] Created (runtime=%p)", TAG, &runtime);
}

std::shared_ptr<RuntimeContext> RuntimeContext::get(jsi::Runtime &runtime) {
  auto data = runtime.getRuntimeData(runtimeDataUUID());
  if (!data) {
    return nullptr;
  }
  return std::static_pointer_cast<RuntimeData>(data)->context;
}

std::shared_ptr<RuntimeContext>
RuntimeContext::getOrCreate(jsi::Runtime &runtime, wgpu::Instance instance) {
  if (auto existing = get(runtime)) {
    return existing;
  }
  auto context = std::make_shared<RuntimeContext>(runtime, std::move(instance));
  auto data = std::make_shared<RuntimeData>();
  data->context = context;
  runtime.setRuntimeData(runtimeDataUUID(), data);
  return context;
}

AsyncTaskHandle RuntimeContext::postTask(const TaskCallback &callback,
                                         bool keepPumping) {
  auto handle = AsyncTaskHandle::create(shared_from_this(), keepPumping);
  if (!handle.valid()) {
    throw std::runtime_error("Failed to create AsyncTaskHandle.");
  }

  _pendingTasks.fetch_add(1, std::memory_order_acq_rel);
  if (keepPumping) {
    _pumpTasks.fetch_add(1, std::memory_order_acq_rel);
  }
  requestTick();

  auto resolve = handle.createResolveFunction();
  auto reject = handle.createRejectFunction();
  try {
    callback(resolve, reject);
  } catch (const std::exception &exception) {
    reject(exception.what());
  } catch (...) {
    reject("Unknown native error in RuntimeContext::postTask.");
  }
  return handle;
}

void RuntimeContext::onTaskSettled(bool keepPumping) {
  _pendingTasks.fetch_sub(1, std::memory_order_acq_rel);
  if (keepPumping) {
    _pumpTasks.fetch_sub(1, std::memory_order_acq_rel);
  }
}

void RuntimeContext::postSettle(std::function<void()> job) {
  if (!job) {
    return;
  }
  std::lock_guard<std::mutex> lock(_mailboxMutex);
  _mailbox.push_back(std::move(job));
}

void RuntimeContext::drainMailbox() {
  std::vector<std::function<void()>> jobs;
  {
    std::lock_guard<std::mutex> lock(_mailboxMutex);
    jobs.swap(_mailbox);
  }
  // Run settle-actions on this (the owning) thread, NOT under the ProcessEvents
  // mutex, so JS continuations never execute while the pump lock is held.
  for (auto &job : jobs) {
    job();
  }
}

void RuntimeContext::requestTick() {
  bool expected = false;
  if (!_tickScheduled.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel)) {
    return;
  }

  // postTask and tick both run on the owning runtime's thread, so we can
  // schedule the next tick directly via that runtime's own timer. setTimeout is
  // available on the main RN runtime and on worklet runtimes (backed by the
  // worklets EventLoop); setImmediate / queueMicrotask are fallbacks. We do NOT
  // use queueMicrotask as the primary mechanism: a self-rescheduling microtask
  // never yields the microtask checkpoint, starving the runtime's task loop.
  auto self = shared_from_this();
  jsi::Runtime &rt = _runtime;
  auto tickCallback = jsi::Function::createFromHostFunction(
      rt, jsi::PropNameID::forAscii(rt, "RNWGPUAsyncTick"), 0,
      [self](jsi::Runtime & /*runtime*/, const jsi::Value & /*thisVal*/,
             const jsi::Value * /*args*/, size_t /*count*/) -> jsi::Value {
        self->tick();
        return jsi::Value::undefined();
      });

  auto global = rt.global();
  auto setTimeoutValue = global.getProperty(rt, "setTimeout");
  if (setTimeoutValue.isObject() &&
      setTimeoutValue.asObject(rt).isFunction(rt)) {
    setTimeoutValue.asObject(rt).asFunction(rt).call(
        rt, jsi::Value(rt, tickCallback), jsi::Value(0));
    return;
  }
  auto setImmediateValue = global.getProperty(rt, "setImmediate");
  if (setImmediateValue.isObject() &&
      setImmediateValue.asObject(rt).isFunction(rt)) {
    setImmediateValue.asObject(rt).asFunction(rt).call(
        rt, jsi::Value(rt, tickCallback));
    return;
  }
  rt.queueMicrotask(std::move(tickCallback));
}

void RuntimeContext::tick() {
  _tickScheduled.store(false, std::memory_order_release);
  {
    // Serialize ProcessEvents across runtimes sharing this instance. Callbacks
    // fired here only deposit into mailboxes (postSettle), they do not run JS.
    std::lock_guard<std::mutex> lock(processEventsMutex());
    _instance.ProcessEvents();
  }
  // Settle this runtime's ready promises on this thread, outside the pump lock.
  drainMailbox();
  if (_pumpTasks.load(std::memory_order_acquire) > 0) {
    requestTick();
  }
}

jsi::UUID RuntimeContext::runtimeDataUUID() {
  static const auto uuid = jsi::UUID();
  return uuid;
}

} // namespace rnwgpu::async
