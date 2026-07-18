#include "AsyncTaskHandle.h"

#include <exception>
#include <memory>
#include <string>
#include <utility>

#include <ReactCommon/CallInvoker.h>

#include "Promise.h"
#include "RuntimeContext.h"

namespace rnwgpu::async {

namespace {

using Action = std::function<void(jsi::Runtime &, rnwgpu::Promise &)>;

} // namespace

struct AsyncTaskHandle::State final
    : public std::enable_shared_from_this<AsyncTaskHandle::State> {
  State(std::weak_ptr<RuntimeContext> context, bool keepPumping) noexcept
      : context(std::move(context)), keepPumping(keepPumping) {}

  void settle(Action action) noexcept;
  void attachPromise(const std::shared_ptr<rnwgpu::Promise> &newPromise);
  void schedule(Action action) noexcept;
  void
  runScheduled(Action &action,
               const std::shared_ptr<rnwgpu::Promise> &promiseRef) noexcept;
  void cancel() noexcept;
  void finish() noexcept;

  ResolveFunction createResolveFunction();
  RejectFunction createRejectFunction();

  std::mutex mutex;
  std::weak_ptr<RuntimeContext> context;
  const bool keepPumping;
  std::shared_ptr<rnwgpu::Promise> promise;
  std::optional<Action> pendingAction;
  bool settled{false};
  bool finished{false};
  bool cancelled{false};
  bool pumpStarted{false};
};

void AsyncTaskHandle::State::settle(Action action) noexcept {
  if (!action) {
    return;
  }

  try {
    std::optional<Action> actionToSchedule;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (settled || cancelled || finished) {
        return;
      }
      settled = true;
      if (promise) {
        actionToSchedule = std::move(action);
      } else {
        pendingAction = std::move(action);
      }
    }

    if (actionToSchedule.has_value()) {
      schedule(std::move(*actionToSchedule));
    }
  } catch (...) {
    // Dawn callbacks must never receive a C++ exception.
  }
}

void AsyncTaskHandle::State::attachPromise(
    const std::shared_ptr<rnwgpu::Promise> &newPromise) {
  if (!newPromise) {
    cancel();
    return;
  }

  auto contextRef = context.lock();
  if (!contextRef ||
      !newPromise->belongsToRuntime(contextRef->runtimeIdentity())) {
    newPromise->reject(
        "Asynchronous WebGPU objects must be used on their calling runtime");
    cancel();
    return;
  }

  if (keepPumping) {
    try {
      if (!contextRef->beginPumping()) {
        newPromise->reject("WebGPU runtime was invalidated");
        cancel();
        return;
      }
    } catch (const std::exception &exception) {
      newPromise->reject(exception.what());
      cancel();
      return;
    } catch (...) {
      newPromise->reject("Failed to start the WebGPU event pump");
      cancel();
      return;
    }
  }

  std::optional<Action> actionToRun;
  bool rollbackPump = false;
  bool unavailable = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (cancelled || finished) {
      rollbackPump = keepPumping;
      unavailable = true;
    } else {
      pumpStarted = keepPumping;
      promise = newPromise;
      if (pendingAction.has_value()) {
        actionToRun = std::move(pendingAction);
        pendingAction.reset();
      }
    }
  }

  if (rollbackPump) {
    contextRef->onTaskSettled(/*keepPumping=*/true);
  }
  if (unavailable) {
    newPromise->reject("WebGPU runtime was invalidated");
    return;
  }

  if (actionToRun.has_value()) {
    // The Promise executor runs on the owning runtime. A Dawn operation that
    // completed synchronously before attachment can settle directly here.
    runScheduled(*actionToRun, newPromise);
  }
}

void AsyncTaskHandle::State::schedule(Action action) noexcept {
  try {
    std::shared_ptr<rnwgpu::Promise> promiseRef;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (cancelled || finished || !promise) {
        return;
      }
      promiseRef = promise;
    }

    auto contextRef = context.lock();
    if (!contextRef || !contextRef->isValid()) {
      return;
    }

    auto self = shared_from_this();
    if (!keepPumping) {
      const auto invoker = contextRef->callInvoker();
      if (invoker) {
        invoker->invokeAsync(
            [self, action = std::move(action), promiseRef]() mutable {
              self->runScheduled(action, promiseRef);
            });
      }
      // Worklet runtimes have no native dispatcher for spontaneous events.
      // Keep the task runtime-owned so teardown can release it safely.
      return;
    }

    contextRef->postSettle(
        [self, action = std::move(action), promiseRef]() mutable {
          self->runScheduled(action, promiseRef);
        });
  } catch (...) {
    // Allocation/dispatcher failures must not unwind through a Dawn callback.
  }
}

void AsyncTaskHandle::State::runScheduled(
    Action &action,
    const std::shared_ptr<rnwgpu::Promise> &promiseRef) noexcept {
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (cancelled || finished) {
      return;
    }
  }

  const auto contextRef = context.lock();
  if (!contextRef) {
    finish();
    return;
  }

  bool ranOnOwningRuntime = false;
  try {
    ranOnOwningRuntime = contextRef->withRuntime([&](jsi::Runtime &runtime) {
      try {
        action(runtime, *promiseRef);
      } catch (const std::exception &exception) {
        promiseRef->reject(exception.what());
      } catch (...) {
        promiseRef->reject(
            "Unknown native error while settling WebGPU Promise");
      }
    });
  } catch (...) {
    // CallInvoker/timer callbacks must never propagate a native exception.
  }

  if (!ranOnOwningRuntime) {
    // This callback is already executing on the task's owning runtime thread.
    // If the session became inactive before the callback ran, release the JSI
    // resolver/rejecter before unregistering the task. finish() alone would
    // leave PromiseRuntimeContext as the sole owner until runtime destruction.
    cancel();
    return;
  }
  finish();
}

void AsyncTaskHandle::State::cancel() noexcept {
  std::shared_ptr<rnwgpu::Promise> promiseToInvalidate;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (cancelled || finished) {
      return;
    }
    cancelled = true;
    promiseToInvalidate = std::move(promise);
    pendingAction.reset();
  }

  // cancel() is called by RuntimeContext's runtimeData teardown path, so this
  // is the owning runtime thread and JSI function destruction is safe.
  if (promiseToInvalidate) {
    promiseToInvalidate->invalidate();
  }
  finish();
}

void AsyncTaskHandle::State::finish() noexcept {
  std::shared_ptr<RuntimeContext> contextRef;
  bool decrementPump = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (finished) {
      return;
    }
    finished = true;
    decrementPump = keepPumping && pumpStarted;
    promise.reset();
    pendingAction.reset();
    contextRef = context.lock();
  }

  if (contextRef) {
    contextRef->onTaskSettled(decrementPump);
    contextRef->unregisterTask(this);
  }
}

AsyncTaskHandle::ResolveFunction
AsyncTaskHandle::State::createResolveFunction() {
  const auto weakSelf = std::weak_ptr<State>(shared_from_this());
  return [weakSelf](ValueFactory factory) {
    if (auto self = weakSelf.lock()) {
      ValueFactory resolvedFactory =
          factory ? std::move(factory) : [](jsi::Runtime & /*runtime*/) {
            return jsi::Value::undefined();
          };
      self->settle(
          [factory = std::move(resolvedFactory)](
              jsi::Runtime &runtime, rnwgpu::Promise &promise) mutable {
            promise.resolve(jsi::Value(factory(runtime)));
          });
    }
  };
}

AsyncTaskHandle::RejectFunction AsyncTaskHandle::State::createRejectFunction() {
  const auto weakSelf = std::weak_ptr<State>(shared_from_this());
  return [weakSelf](std::string reason) {
    if (auto self = weakSelf.lock()) {
      self->settle([reason = std::move(reason)](jsi::Runtime & /*runtime*/,
                                                rnwgpu::Promise &promise) {
        promise.reject(reason);
      });
    }
  };
}

AsyncTaskHandle::AsyncTaskHandle(std::shared_ptr<State> state)
    : _state(std::move(state)) {}

bool AsyncTaskHandle::valid() const noexcept { return _state != nullptr; }

AsyncTaskHandle
AsyncTaskHandle::create(const std::shared_ptr<RuntimeContext> &context,
                        bool keepPumping) {
  if (!context) {
    return {};
  }

  auto state = std::make_shared<State>(context, keepPumping);
  const auto weakState = std::weak_ptr<State>(state);
  const bool registered =
      context->registerTask(state.get(), state, [weakState]() {
        if (const auto task = weakState.lock()) {
          task->cancel();
        }
      });
  if (!registered) {
    return {};
  }
  return AsyncTaskHandle(std::move(state));
}

AsyncTaskHandle::ResolveFunction
AsyncTaskHandle::createResolveFunction() const {
  if (!_state) {
    return [](ValueFactory) {};
  }
  return _state->createResolveFunction();
}

AsyncTaskHandle::RejectFunction AsyncTaskHandle::createRejectFunction() const {
  if (!_state) {
    return [](std::string) {};
  }
  return _state->createRejectFunction();
}

void AsyncTaskHandle::attachPromise(
    const std::shared_ptr<rnwgpu::Promise> &promise) const {
  if (_state) {
    _state->attachPromise(promise);
  }
}

void AsyncTaskHandle::cancel() const noexcept {
  if (_state) {
    _state->cancel();
  }
}

} // namespace rnwgpu::async
