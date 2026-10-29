#include "RuntimeContext.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(__APPLE__) || defined(__ANDROID__)
#include <pthread.h>
#endif

#include <ReactCommon/CallInvoker.h>

#include "AsyncTaskHandle.h"
#include "WGPULogger.h"

namespace rnwgpu::async {

namespace {
struct RuntimeData {
  std::unordered_map<WGPUInstance, std::shared_ptr<RuntimeContext>> contexts;
};
constexpr const char *TAG = "RuntimeContext";

// The main JS runtime and its CallInvoker, registered once on install.
jsi::Runtime *sMainRuntime = nullptr;
std::shared_ptr<facebook::react::CallInvoker> sMainInvoker;

/**
 * The thread waiting on the futures of one instance: one future at a time,
 * in the order they were handed over, in Instance::WaitAny, which sleeps
 * until the GPU or Dawn's worker is done and then runs the callback. Never
 * destroyed: the instance it retains is shared by every runtime, and a
 * future may be outstanding during process teardown.
 */
class Waiter {
public:
  static Waiter &forInstance(const wgpu::Instance &instance) {
    static std::mutex registryMutex;
    static std::unordered_map<WGPUInstance, Waiter *> registry;
    std::lock_guard<std::mutex> lock(registryMutex);
    auto &slot = registry[instance.Get()];
    if (slot == nullptr) {
      slot = new Waiter(instance);
    }
    return *slot;
  }

  void enqueue(wgpu::Future future) {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _futures.push_back(future);
    }
    _condition.notify_one();
  }

private:
  explicit Waiter(wgpu::Instance instance) : _instance(std::move(instance)) {
    std::thread([this]() { run(); }).detach();
  }

  void run() {
#if defined(__APPLE__)
    pthread_setname_np("RNWebGPU Waiter");
#elif defined(__ANDROID__)
    pthread_setname_np(pthread_self(), "RNWebGPU Waiter");
#endif
    for (;;) {
      wgpu::Future future;
      {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this]() { return !_futures.empty(); });
        future = _futures.front();
        _futures.pop_front();
      }
      auto status = _instance.WaitAny(future, UINT64_MAX);
      if (status != wgpu::WaitStatus::Success) {
        Logger::logToConsole("[%s] WaitAny failed with status %d", TAG,
                             static_cast<int>(status));
      }
    }
  }

  wgpu::Instance _instance;
  std::mutex _mutex;
  std::condition_variable _condition;
  std::deque<wgpu::Future> _futures;
};
} // namespace

void RuntimeContext::registerMainRuntime(
    jsi::Runtime *runtime,
    std::shared_ptr<facebook::react::CallInvoker> invoker) {
  sMainRuntime = runtime;
  sMainInvoker = std::move(invoker);
}

RuntimeContext::Kind RuntimeContext::kindOf(jsi::Runtime &runtime) {
  if (&runtime == sMainRuntime) {
    return Kind::Main;
  }
  // react-native-worklets' RuntimeKind: 1 for React Native, 2 for the UI
  // runtime, more for the runtimes of the app.
  auto kind = runtime.global().getProperty(runtime, "__RUNTIME_KIND");
  if (!kind.isNumber() || kind.asNumber() == 1) {
    return Kind::Main;
  }
  return kind.asNumber() == 2 ? Kind::UI : Kind::Mailbox;
}

RuntimeContext::RuntimeContext(jsi::Runtime &runtime, wgpu::Instance instance)
    : _runtime(runtime), _instance(std::move(instance)),
      _kind(kindOf(runtime)) {
  Logger::logToConsole("[%s] Created (runtime=%p)", TAG, &runtime);
}

std::shared_ptr<RuntimeContext>
RuntimeContext::getOrCreate(jsi::Runtime &runtime,
                            const wgpu::Instance &instance) {
  auto data = std::static_pointer_cast<RuntimeData>(
      runtime.getRuntimeData(runtimeDataUUID()));
  if (!data) {
    data = std::make_shared<RuntimeData>();
    runtime.setRuntimeData(runtimeDataUUID(), data);
  }
  // Contexts retain their instance, so its handle is a stable key for the
  // runtime's lifetime. A slot left null by a throwing constructor is simply
  // filled next time.
  auto &context = data->contexts[instance.Get()];
  if (!context) {
    context = std::make_shared<RuntimeContext>(runtime, instance);
  }
  return context;
}

AsyncTaskHandle RuntimeContext::postTask(const TaskCallback &callback) {
  auto handle = AsyncTaskHandle::create(shared_from_this());
  if (!handle.valid()) {
    throw std::runtime_error("Failed to create AsyncTaskHandle.");
  }
  // A runtime that polls its mailbox starts doing so now, on its own thread;
  // the settle action ends the operation (onTaskSettled).
  _outstanding.fetch_add(1, std::memory_order_acq_rel);
  if (_kind == Kind::Mailbox) {
    armTick(_runtime);
  }

  auto resolve = handle.createResolveFunction();
  auto reject = handle.createRejectFunction();
  try {
    auto future = callback(resolve, reject);
    if (future.has_value()) {
      awaitFuture(*future);
    }
  } catch (const std::exception &exception) {
    reject(exception.what());
  } catch (...) {
    reject("Unknown native error in RuntimeContext::postTask.");
  }
  return handle;
}

void RuntimeContext::awaitFuture(wgpu::Future future) {
  // Complete already (requestAdapter, popErrorScope, a lost device): the
  // callback runs here, and nothing waits.
  if (_instance.WaitAny(future, 0) == wgpu::WaitStatus::Success) {
    return;
  }
  Waiter::forInstance(_instance).enqueue(future);
}

void RuntimeContext::onTaskSettled() {
  _outstanding.fetch_sub(1, std::memory_order_acq_rel);
}

void RuntimeContext::post(Job job) {
  switch (_kind) {
  case Kind::Main:
    if (sMainInvoker == nullptr) {
      return;
    }
    sMainInvoker->invokeAsync(facebook::react::CallFunc(std::move(job)));
    return;
  case Kind::UI:
    if (sMainInvoker == nullptr) {
      return;
    }
    // On the main runtime: worklets runs the function handed to
    // __webgpuScheduleOnUI on the UI runtime. It is a host function, which
    // worklets passes by reference.
    sMainInvoker->invokeAsync(facebook::react::CallFunc(
        [job = std::move(job)](jsi::Runtime &mainRuntime) mutable {
          auto schedule = mainRuntime.global().getProperty(
              mainRuntime, "__webgpuScheduleOnUI");
          if (!schedule.isObject() ||
              !schedule.asObject(mainRuntime).isFunction(mainRuntime)) {
            Logger::logToConsole(
                "[%s] Nothing can run on the UI runtime: react-native-worklets "
                "did not install __webgpuScheduleOnUI (see "
                "registerWebGPUForReanimated).",
                TAG);
            return;
          }
          auto function = jsi::Function::createFromHostFunction(
              mainRuntime, jsi::PropNameID::forAscii(mainRuntime, "webgpuJob"),
              0,
              [job = std::move(job)](jsi::Runtime &uiRuntime,
                                     const jsi::Value &, const jsi::Value *,
                                     size_t) -> jsi::Value {
                job(uiRuntime);
                return jsi::Value::undefined();
              });
          schedule.asObject(mainRuntime)
              .asFunction(mainRuntime)
              .call(mainRuntime, std::move(function));
        }));
    return;
  case Kind::Mailbox: {
    std::lock_guard<std::mutex> lock(_mailboxMutex);
    _mailbox.push_back(std::move(job));
    return;
  }
  }
}

void RuntimeContext::armTick(jsi::Runtime &runtime) {
  bool expected = false;
  if (!_tickArmed.compare_exchange_strong(expected, true,
                                          std::memory_order_acq_rel)) {
    return;
  }
  // The runtime's own timer, on its own thread. Not queueMicrotask: a
  // self-rescheduling microtask never yields the microtask checkpoint, which
  // starves the runtime's task loop.
  auto global = runtime.global();
  auto setTimeout = global.getProperty(runtime, "setTimeout");
  if (!setTimeout.isObject() ||
      !setTimeout.asObject(runtime).isFunction(runtime)) {
    _tickArmed.store(false, std::memory_order_release);
    Logger::logToConsole("[%s] This runtime has no setTimeout: nothing "
                         "asynchronous can settle on it.",
                         TAG);
    return;
  }
  auto self = shared_from_this();
  auto tick = jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forAscii(runtime, "RNWGPUAsyncTick"), 0,
      [self](jsi::Runtime &runtime, const jsi::Value &, const jsi::Value *,
             size_t) -> jsi::Value {
        self->_tickArmed.store(false, std::memory_order_release);
        self->drainMailbox(runtime);
        if (self->_outstanding.load(std::memory_order_acquire) > 0) {
          self->armTick(runtime);
        }
        return jsi::Value::undefined();
      });
  setTimeout.asObject(runtime).asFunction(runtime).call(
      runtime, std::move(tick), jsi::Value(0.0));
}

void RuntimeContext::drainMailbox(jsi::Runtime &runtime) {
  std::vector<Job> jobs;
  {
    std::lock_guard<std::mutex> lock(_mailboxMutex);
    jobs.swap(_mailbox);
  }
  for (auto &job : jobs) {
    job(runtime);
  }
}

jsi::UUID RuntimeContext::runtimeDataUUID() {
  // Fixed random UUID identifying react-native-webgpu's runtimeData slot.
  // Never use the default-constructed all-zero UUID here: Hermes backs
  // runtimeData with a DenseMap that reserves the all-zero UUID as its
  // empty-bucket marker (and the all-0xff UUID as its tombstone), so using
  // either as a key is undefined behavior. It crashes on worklet runtimes,
  // where react-native-worklets seeds runtimeData before we do.
  static constexpr jsi::UUID uuid(0xC06D5D2D, 0xB19B, 0x431B, 0xA89E,
                                  0x20F8A0B6E31BULL);
  return uuid;
}

} // namespace rnwgpu::async
