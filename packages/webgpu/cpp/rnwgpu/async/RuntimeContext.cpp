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

// The main JS runtime and its CallInvoker, registered on install (again
// after a reload) on the JS thread, and read from any thread: the waiter
// thread posts through them.
std::mutex sMainMutex;
jsi::Runtime *sMainRuntime = nullptr;
std::shared_ptr<facebook::react::CallInvoker> sMainInvoker;
// Whether a wake that could not run has been reported: once per process.
std::atomic<bool> sWakeLogged{false};

jsi::Runtime *mainRuntime() {
  std::lock_guard<std::mutex> lock(sMainMutex);
  return sMainRuntime;
}

std::shared_ptr<facebook::react::CallInvoker> mainInvoker() {
  std::lock_guard<std::mutex> lock(sMainMutex);
  return sMainInvoker;
}

void logException(const char *what, const std::exception_ptr &error) {
  try {
    std::rethrow_exception(error);
  } catch (const std::exception &exception) {
    Logger::logToConsole("[%s] %s: %s", TAG, what, exception.what());
  } catch (...) {
    Logger::logToConsole("[%s] %s.", TAG, what);
  }
}

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
  std::lock_guard<std::mutex> lock(sMainMutex);
  sMainRuntime = runtime;
  sMainInvoker = std::move(invoker);
}

RuntimeContext::Kind RuntimeContext::kindOf(jsi::Runtime &runtime) {
  auto main = mainRuntime();
  if (&runtime == main) {
    return Kind::Main;
  }
  // react-native-worklets' RuntimeKind: 1 for React Native, 2 for the UI
  // runtime, more for the runtimes of the app.
  auto kind = runtime.global().getProperty(runtime, "__RUNTIME_KIND");
  if (kind.isNumber() && kind.asNumber() == 2) {
    return Kind::UI;
  }
  if (main == nullptr && (!kind.isNumber() || kind.asNumber() == 1)) {
    // No main runtime registered (yet): the React Native runtime, as far as
    // anything can tell.
    return Kind::Main;
  }
  // Another runtime, marked or not: settling it through the main runtime's
  // CallInvoker would settle it with the main runtime while its promise
  // lives on this one. Its own setTimeout reaches it.
  return Kind::Mailbox;
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

bool RuntimeContext::canSettle() const {
  switch (_kind) {
  case Kind::Main:
    return mainInvoker() != nullptr;
  case Kind::UI:
  case Kind::Mailbox:
    return true;
  }
  return false;
}

AsyncTaskHandle RuntimeContext::postTask(const TaskCallback &callback) {
  if (!canSettle()) {
    throw std::runtime_error("Nothing can settle a promise on this runtime: "
                             "react-native-webgpu is not installed on it.");
  }
  // A runtime that polls its mailbox starts doing so now, on its own thread;
  // the settle action ends the operation (onTaskSettled). Without a timer
  // nothing could ever settle the promise: none is handed out.
  if (_kind != Kind::Main && !armTick(_runtime)) {
    throw std::runtime_error("Nothing can settle a promise on this runtime: "
                             "it has no setTimeout.");
  }
  auto handle = AsyncTaskHandle::create(shared_from_this());
  if (!handle.valid()) {
    throw std::runtime_error("Failed to create AsyncTaskHandle.");
  }
  _outstanding.fetch_add(1, std::memory_order_acq_rel);

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
  case Kind::Main: {
    auto invoker = mainInvoker();
    if (invoker == nullptr) {
      return;
    }
    invoker->invokeAsync(facebook::react::CallFunc(std::move(job)));
    return;
  }
  case Kind::UI:
  case Kind::Mailbox: {
    {
      std::lock_guard<std::mutex> lock(_mailboxMutex);
      _mailbox.push_back(std::move(job));
    }
    if (_kind == Kind::UI) {
      wakeUI();
    }
    return;
  }
  }
}

// Best effort, and coalesced (one wake in flight at a time): on the main
// runtime, worklets runs the function handed to __webgpuScheduleOnUI on the
// UI runtime, where it drains the mailbox ahead of the next tick. The
// function is a host function, which worklets passes by reference. Without
// the hook (registerWebGPUForReanimated has not run on this main runtime
// yet, a reload evaluating its bundle, say) the tick alone settles what is
// awaited, a spontaneous event waits for the next awaited operation, and
// the next post tries again. Whatever goes wrong on the way, the wake is
// given up, never the jobs.
void RuntimeContext::wakeUI() {
  auto invoker = mainInvoker();
  if (invoker == nullptr) {
    return;
  }
  bool expected = false;
  if (!_wakePending.compare_exchange_strong(expected, true,
                                            std::memory_order_acq_rel)) {
    return;
  }
  std::weak_ptr<RuntimeContext> weak = weak_from_this();
  invoker->invokeAsync(
      facebook::react::CallFunc([weak](jsi::Runtime &mainRuntime) {
        auto self = weak.lock();
        if (self == nullptr) {
          return;
        }
        try {
          auto schedule = mainRuntime.global().getProperty(
              mainRuntime, "__webgpuScheduleOnUI");
          if (!schedule.isObject() ||
              !schedule.asObject(mainRuntime).isFunction(mainRuntime)) {
            self->_wakePending.store(false, std::memory_order_release);
            if (!sWakeLogged.exchange(true, std::memory_order_acq_rel)) {
              Logger::logToConsole(
                  "[%s] __webgpuScheduleOnUI is not installed on the main "
                  "runtime (see registerWebGPUForReanimated): the UI "
                  "runtime settles its promises on its own timer for now, "
                  "and device.lost and uncapturederror reach it with its "
                  "next awaited operation.",
                  TAG);
            }
            return;
          }
          auto wake = jsi::Function::createFromHostFunction(
              mainRuntime, jsi::PropNameID::forAscii(mainRuntime, "webgpuWake"),
              0,
              [weak](jsi::Runtime &uiRuntime, const jsi::Value &,
                     const jsi::Value *, size_t) -> jsi::Value {
                auto self = weak.lock();
                // The UI runtime of the moment: after a reload, not the one
                // this mailbox belongs to.
                if (self == nullptr || &uiRuntime != &self->_runtime) {
                  return jsi::Value::undefined();
                }
                // Cleared before the drain: a job posted meanwhile is either
                // in this drain or gets a wake of its own.
                self->_wakePending.store(false, std::memory_order_release);
                auto error = self->drainMailbox(uiRuntime);
                if (error != nullptr) {
                  // Reported, not rethrown: the wake runs among the other
                  // jobs of worklets' scheduleOnUI batch, which the
                  // exception would cut short. (The tick rethrows its own
                  // as the timer's.)
                  logException("A job run by the wake on the UI runtime threw",
                               error);
                }
                return jsi::Value::undefined();
              });
          schedule.asObject(mainRuntime)
              .asFunction(mainRuntime)
              .call(mainRuntime, std::move(wake));
        } catch (const std::exception &error) {
          self->_wakePending.store(false, std::memory_order_release);
          if (!sWakeLogged.exchange(true, std::memory_order_acq_rel)) {
            Logger::logToConsole(
                "[%s] Waking the UI runtime through __webgpuScheduleOnUI "
                "failed, so it settles its promises on its own timer for "
                "now: %s",
                TAG, error.what());
          }
        }
      }));
}

bool RuntimeContext::armTick(jsi::Runtime &runtime) {
  bool expected = false;
  if (!_tickArmed.compare_exchange_strong(expected, true,
                                          std::memory_order_acq_rel)) {
    return true;
  }
  // The runtime's own timer, on its own thread. Not queueMicrotask: a
  // self-rescheduling microtask never yields the microtask checkpoint, which
  // starves the runtime's task loop.
  try {
    auto setTimeout = runtime.global().getProperty(runtime, "setTimeout");
    if (!setTimeout.isObject() ||
        !setTimeout.asObject(runtime).isFunction(runtime)) {
      _tickArmed.store(false, std::memory_order_release);
      Logger::logToConsole("[%s] This runtime has no setTimeout: nothing "
                           "asynchronous can settle on it.",
                           TAG);
      return false;
    }
    std::weak_ptr<RuntimeContext> weak = weak_from_this();
    auto tick = jsi::Function::createFromHostFunction(
        runtime, jsi::PropNameID::forAscii(runtime, "RNWGPUAsyncTick"), 0,
        [weak](jsi::Runtime &runtime, const jsi::Value &, const jsi::Value *,
               size_t) -> jsi::Value {
          auto self = weak.lock();
          if (self == nullptr) {
            return jsi::Value::undefined();
          }
          self->_tickArmed.store(false, std::memory_order_release);
          auto error = self->drainMailbox(runtime);
          if (self->_outstanding.load(std::memory_order_acquire) > 0) {
            self->armTick(runtime);
          }
          if (error != nullptr) {
            // Thrown by a job, after the others ran and the next tick is
            // armed: the runtime reports it as the timer's.
            std::rethrow_exception(error);
          }
          return jsi::Value::undefined();
        });
    setTimeout.asObject(runtime).asFunction(runtime).call(
        runtime, std::move(tick), jsi::Value(0.0));
    return true;
  } catch (const std::exception &error) {
    _tickArmed.store(false, std::memory_order_release);
    Logger::logToConsole("[%s] This runtime's setTimeout threw, so nothing "
                         "asynchronous can settle on it: %s",
                         TAG, error.what());
    return false;
  }
}

// Runs the jobs posted so far, every one of them even when one throws: the
// first exception is returned for the caller to rethrow once the rest ran.
std::exception_ptr RuntimeContext::drainMailbox(jsi::Runtime &runtime) {
  std::vector<Job> jobs;
  {
    std::lock_guard<std::mutex> lock(_mailboxMutex);
    jobs.swap(_mailbox);
  }
  std::exception_ptr first = nullptr;
  for (auto &job : jobs) {
    try {
      job(runtime);
    } catch (...) {
      if (first == nullptr) {
        first = std::current_exception();
      }
    }
  }
  return first;
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
