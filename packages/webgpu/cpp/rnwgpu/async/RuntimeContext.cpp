#include "RuntimeContext.h"

#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ReactCommon/CallInvoker.h>

#include "AsyncTaskHandle.h"
#include "WGPULogger.h"

namespace rnwgpu::async {

namespace {

struct RuntimeData final {
  explicit RuntimeData(std::shared_ptr<RuntimeContext> context)
      : context(std::move(context)) {}

  ~RuntimeData() {
    if (context) {
      context->invalidate();
    }
  }

  std::shared_ptr<RuntimeContext> context;
};

struct MainRuntimeRegistration final {
  jsi::Runtime *runtime{nullptr};
  std::shared_ptr<facebook::react::CallInvoker> invoker;
  std::weak_ptr<RuntimeContext> context;
  const RuntimeContext *contextIdentity{nullptr};
};

struct MainRuntimeRegistry final {
  std::mutex mutex;
  std::unordered_map<RNWebGPUSessionId, MainRuntimeRegistration> registrations;
};

MainRuntimeRegistry &mainRuntimeRegistry() {
  // Runtime teardown may run during process-wide static destruction.
  static auto *registry = new MainRuntimeRegistry();
  return *registry;
}

void unregisterRuntime(jsi::Runtime *runtimeIdentity,
                       const RuntimeContext *contextIdentity) noexcept {
  if (runtimeIdentity == nullptr && contextIdentity == nullptr) {
    return;
  }

  auto &registry = mainRuntimeRegistry();
  // Move owning fields out before erasing so CallInvoker destructors never run
  // under the registry mutex. Reserve can fail only under extreme memory
  // pressure; the fallback still removes every stale identity atomically, but
  // may release an invoker while holding the mutex.
  std::vector<MainRuntimeRegistration> removed;
  {
    std::lock_guard<std::mutex> lock(registry.mutex);
    std::size_t matchingCount = 0;
    for (const auto &[sessionId, registration] : registry.registrations) {
      (void)sessionId;
      const bool matchesRuntime =
          runtimeIdentity != nullptr && registration.runtime == runtimeIdentity;
      const bool matchesContext =
          contextIdentity != nullptr &&
          registration.contextIdentity == contextIdentity;
      if (matchesRuntime || matchesContext) {
        ++matchingCount;
      }
    }

    bool canMoveOut = true;
    try {
      removed.reserve(matchingCount);
    } catch (...) {
      canMoveOut = false;
    }

    for (auto it = registry.registrations.begin();
         it != registry.registrations.end();) {
      const bool matchesRuntime =
          runtimeIdentity != nullptr && it->second.runtime == runtimeIdentity;
      const bool matchesContext = contextIdentity != nullptr &&
                                  it->second.contextIdentity == contextIdentity;
      if (!matchesRuntime && !matchesContext) {
        ++it;
        continue;
      }
      if (canMoveOut) {
        removed.push_back(std::move(it->second));
      }
      it = registry.registrations.erase(it);
    }
  }
}

std::mutex &processEventsMutex() {
  static auto *mutex = new std::mutex();
  return *mutex;
}

constexpr const char *kLogTag = "RuntimeContext";

} // namespace

void RuntimeContext::registerMainRuntime(
    RNWebGPUSessionId sessionId, jsi::Runtime *runtime,
    std::shared_ptr<facebook::react::CallInvoker> invoker) {
  if (sessionId == kInvalidRNWebGPUSessionId || runtime == nullptr ||
      !invoker) {
    throw std::invalid_argument(
        "Main WebGPU runtime registration requires a valid session, runtime, "
        "and CallInvoker");
  }

  // This is the only JSI operation below. Perform it before publishing the
  // process-wide registration so an exception cannot leave a stale raw
  // runtime identity behind.
  const auto context = get(*runtime);

  auto &registry = mainRuntimeRegistry();
  MainRuntimeRegistration newRegistration{
      runtime, std::move(invoker), {}, nullptr};
  MainRuntimeRegistration oldRegistration;
  std::shared_ptr<RuntimeContext> replacedContext;
  {
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto existing = registry.registrations.find(sessionId);
    if (existing != registry.registrations.end()) {
      replacedContext = existing->second.context.lock();
      oldRegistration = std::move(existing->second);
      existing->second = std::move(newRegistration);
    } else {
      registry.registrations.emplace(sessionId, std::move(newRegistration));
    }
  }

  // registerMainRuntime is called by module installation on the JS thread, so
  // it may safely inspect runtimeData. Only bind a context created for this
  // exact session; a persistent runtime may still contain the previous
  // session until the first operation from the newly installed GPU replaces
  // it.
  if (context) {
    std::shared_ptr<facebook::react::CallInvoker> dispatcher;
    {
      std::lock_guard<std::mutex> lock(registry.mutex);
      const auto current = registry.registrations.find(sessionId);
      if (current != registry.registrations.end() &&
          current->second.runtime == runtime) {
        dispatcher = current->second.invoker;
      }
    }
    bool contextMatchesSession = false;
    if (dispatcher) {
      std::lock_guard<std::recursive_mutex> lock(context->_lifecycleMutex);
      if (context->_runtime == runtime && context->_sessionState &&
          context->_sessionState->id() == sessionId &&
          context->_sessionState->isActive()) {
        context->_mainSessionId = sessionId;
        context->_callInvoker = std::move(dispatcher);
        contextMatchesSession = true;
      }
    }

    if (contextMatchesSession && context->ownsRuntime(*runtime)) {
      std::lock_guard<std::mutex> lock(registry.mutex);
      const auto current = registry.registrations.find(sessionId);
      if (current != registry.registrations.end() &&
          current->second.runtime == runtime) {
        current->second.context = context;
        current->second.contextIdentity = context.get();
      }
    }
  }

  if (replacedContext && replacedContext != context) {
    replacedContext->detachMainRuntime(sessionId);
  }
}

void RuntimeContext::unregisterMainRuntime(
    RNWebGPUSessionId sessionId) noexcept {
  if (sessionId == kInvalidRNWebGPUSessionId) {
    return;
  }

  auto &registry = mainRuntimeRegistry();
  std::shared_ptr<RuntimeContext> context;
  MainRuntimeRegistration removedRegistration;
  {
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto registration = registry.registrations.find(sessionId);
    if (registration == registry.registrations.end()) {
      return;
    }
    removedRegistration = std::move(registration->second);
    context = removedRegistration.context.lock();
    registry.registrations.erase(registration);
  }

  if (context) {
    context->detachMainRuntime(sessionId);
    const auto weakContext = std::weak_ptr<RuntimeContext>(context);
    if (removedRegistration.invoker) {
      try {
        removedRegistration.invoker->invokeAsync(
            [weakContext, sessionId](jsi::Runtime &runtime) noexcept {
              const auto context = weakContext.lock();
              if (!context || !context->ownsRuntime(runtime)) {
                return;
              }
              const auto state = context->sessionState();
              if (state && state->id() == sessionId && !state->isActive()) {
                // This callback executes on the owning JSI thread, so it can
                // safely release Promise/listener values retained by the old
                // session. Do not purge registrations by runtime identity: a
                // newer session may already use the same persistent runtime.
                context->invalidateForReplacement();
              }
            });
      } catch (...) {
        // A queued pump tick, runtimeData teardown, or the next session's
        // getOrCreate() will perform the same owning-thread cleanup.
      }
    }
  }
}

RuntimeContext::RuntimeContext(
    jsi::Runtime &runtime, wgpu::Instance instance,
    std::shared_ptr<RNWebGPUSessionState> sessionState)
    : _runtime(&runtime), _instance(std::move(instance)),
      _sessionState(std::move(sessionState)) {
  if (!_sessionState || !_sessionState->isActive()) {
    throw std::invalid_argument(
        "RuntimeContext requires an active WebGPU session");
  }
  Logger::logToConsole("[%s] Created", kLogTag);
}

RuntimeContext::~RuntimeContext() { invalidate(); }

std::shared_ptr<RuntimeContext> RuntimeContext::get(jsi::Runtime &runtime) {
  const auto data = runtime.getRuntimeData(runtimeDataUUID());
  if (!data) {
    return nullptr;
  }
  return std::static_pointer_cast<RuntimeData>(data)->context;
}

std::shared_ptr<RuntimeContext> RuntimeContext::getOrCreate(
    jsi::Runtime &runtime, wgpu::Instance instance,
    std::shared_ptr<RNWebGPUSessionState> sessionState) {
  if (!sessionState || !sessionState->isActive()) {
    throw jsi::JSError(runtime, "WebGPU runtime session is no longer active");
  }
  if (!instance) {
    throw std::runtime_error(
        "Cannot bind a WebGPU runtime to an invalid Dawn Instance");
  }
  const auto sessionId = sessionState->id();

  if (auto existing = get(runtime)) {
    bool hasRuntime = false;
    bool valid = false;
    bool ownsRuntime = false;
    bool sameInstance = false;
    bool sameSession = false;
    {
      std::lock_guard<std::recursive_mutex> lock(existing->_lifecycleMutex);
      hasRuntime = existing->_runtime != nullptr;
      valid = hasRuntime && existing->_sessionState &&
              existing->_sessionState->isActive();
      ownsRuntime = existing->_runtime == &runtime;
      sameInstance = hasRuntime && existing->_instance.Get() == instance.Get();
      sameSession = hasRuntime && existing->_sessionState == sessionState;
    }

    if (hasRuntime && !ownsRuntime) {
      throw std::logic_error(
          "WebGPU runtimeData contains a context owned by another runtime");
    }
    if (valid && sameInstance && sameSession) {
      return existing;
    }

    // A persistent JSI runtime can survive a native module reinstall while
    // the manager creates a new Dawn Instance. Allocate every replacement
    // owner before invalidating the old context, then clear old JSI state on
    // this owning runtime thread and atomically overwrite runtimeData.
    auto replacement = std::make_shared<RuntimeContext>(
        runtime, std::move(instance), std::move(sessionState));
    auto replacementData = std::make_shared<RuntimeData>(replacement);
    existing->invalidateForReplacement();
    try {
      runtime.setRuntimeData(runtimeDataUUID(), replacementData);
    } catch (...) {
      // The old context is now terminal. Do not leave its raw identity in the
      // main-runtime registry if runtimeData replacement itself failed.
      unregisterRuntime(&runtime, existing.get());
      throw;
    }

    auto &registry = mainRuntimeRegistry();
    {
      std::lock_guard<std::mutex> lock(registry.mutex);
      const auto selected = registry.registrations.find(sessionId);
      if (selected != registry.registrations.end()) {
        if (selected->second.runtime == &runtime) {
          replacement->_mainSessionId = sessionId;
          replacement->_callInvoker = selected->second.invoker;
          selected->second.context = replacement;
          selected->second.contextIdentity = replacement.get();
        }
      }
    }
    return replacement;
  }

  auto context = std::make_shared<RuntimeContext>(runtime, std::move(instance),
                                                  std::move(sessionState));
  runtime.setRuntimeData(runtimeDataUUID(),
                         std::make_shared<RuntimeData>(context));

  auto &registry = mainRuntimeRegistry();
  {
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto selected = registry.registrations.find(sessionId);
    if (selected != registry.registrations.end()) {
      if (selected->second.runtime == &runtime) {
        context->_mainSessionId = sessionId;
        context->_callInvoker = selected->second.invoker;
        selected->second.context = context;
        selected->second.contextIdentity = context.get();
      }
    }
  }
  return context;
}

void RuntimeContext::invalidate() noexcept { invalidateImpl(true); }

void RuntimeContext::invalidateForReplacement() noexcept {
  invalidateImpl(false);
  // A persistent runtime can already be registered for its next session.
  // Remove only entries that still point at this exact context, never every
  // entry sharing the same runtime address.
  unregisterRuntime(nullptr, this);
}

void RuntimeContext::invalidateImpl(bool purgeRegistrations) noexcept {
  jsi::Runtime *runtimeIdentity = nullptr;
  wgpu::Instance instance;
  {
    std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
    if (_runtime == nullptr) {
      return;
    }

    runtimeIdentity = _runtime;
    _runtime = nullptr;
    _mainSessionId = kInvalidRNWebGPUSessionId;
    _callInvoker.reset();
    _tickScheduled.store(false, std::memory_order_release);
    _pumpTasks.store(0, std::memory_order_release);
    instance = std::move(_instance);
  }

  // Remove every generation that still names this runtime/context while its
  // address is unambiguously ours. Old manager teardown then becomes a no-op,
  // and a future Runtime allocated at the same address cannot inherit an old
  // CallInvoker. unregisterRuntime never dereferences runtimeIdentity and does
  // not acquire any RuntimeContext lifecycle lock.
  if (purgeRegistrations) {
    unregisterRuntime(runtimeIdentity, this);
  }

  std::unordered_map<InvalidationCallbackId, std::function<void()>> callbacks;
  {
    std::lock_guard<std::mutex> lock(_invalidationCallbacksMutex);
    callbacks.swap(_invalidationCallbacks);
  }
  for (auto &[id, callback] : callbacks) {
    (void)id;
    if (!callback) {
      continue;
    }
    try {
      callback();
    } catch (...) {
      // RuntimeData teardown must never propagate native exceptions.
    }
  }

  std::vector<std::function<void()>> mailbox;
  {
    std::lock_guard<std::mutex> lock(_mailboxMutex);
    mailbox.swap(_mailbox);
  }
  // Destroy queued closures on the runtime teardown thread. Task ownership
  // below keeps their Promise state alive until cancel() clears its JSI values.
  mailbox.clear();

  std::unordered_map<const void *, ActiveTask> tasks;
  {
    std::lock_guard<std::mutex> lock(_tasksMutex);
    tasks.swap(_activeTasks);
  }
  for (auto &[id, task] : tasks) {
    (void)id;
    if (!task.cancel) {
      continue;
    }
    try {
      task.cancel();
    } catch (...) {
      // RuntimeData teardown must never propagate native exceptions.
    }
  }

  instance = nullptr;
}

bool RuntimeContext::isValid() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
  return _runtime != nullptr && _sessionState && _sessionState->isActive();
}

bool RuntimeContext::ownsRuntime(const jsi::Runtime &runtime) const noexcept {
  std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
  return _runtime == &runtime;
}

const void *RuntimeContext::runtimeIdentity() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
  return _runtime;
}

bool RuntimeContext::withRuntime(
    const std::function<void(jsi::Runtime &)> &action) {
  if (!action) {
    return false;
  }

  jsi::Runtime *runtime = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
    if (_runtime == nullptr || !_sessionState || !_sessionState->isActive()) {
      return false;
    }
    runtime = _runtime;
  }

  // Callers are already on this JSI runtime's owning thread. RuntimeData
  // teardown and replacement also execute on that thread, so the raw runtime
  // cannot disappear until this action returns. Never invoke JavaScript while
  // holding _lifecycleMutex (C++ Core Guidelines CP.22).
  action(*runtime);
  return true;
}

std::shared_ptr<facebook::react::CallInvoker>
RuntimeContext::callInvoker() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
  return _callInvoker;
}

wgpu::Instance RuntimeContext::instance() const {
  std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
  return _instance;
}

std::shared_ptr<RNWebGPUSessionState>
RuntimeContext::sessionState() const noexcept {
  std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
  return _sessionState;
}

AsyncTaskHandle RuntimeContext::postTask(const TaskCallback &callback,
                                         bool keepPumping) {
  if (!callback || !isValid()) {
    throw std::runtime_error(
        "Cannot post a WebGPU task after runtime invalidation");
  }

  auto handle = AsyncTaskHandle::create(shared_from_this(), keepPumping);
  if (!handle.valid()) {
    throw std::runtime_error("Failed to create AsyncTaskHandle");
  }

  auto resolve = handle.createResolveFunction();
  auto reject = handle.createRejectFunction();
  try {
    callback(resolve, reject);
  } catch (const std::exception &exception) {
    reject(exception.what());
  } catch (...) {
    reject("Unknown native error in RuntimeContext::postTask");
  }
  return handle;
}

bool RuntimeContext::beginPumping() {
  {
    std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
    if (_runtime == nullptr || !_sessionState || !_sessionState->isActive()) {
      return false;
    }
    _pumpTasks.fetch_add(1, std::memory_order_acq_rel);
  }

  try {
    requestTick();
  } catch (...) {
    onTaskSettled(/*keepPumping=*/true);
    throw;
  }
  return true;
}

bool RuntimeContext::postSettle(std::function<void()> job) {
  if (!job) {
    return false;
  }

  std::lock_guard<std::recursive_mutex> lifecycleLock(_lifecycleMutex);
  if (_runtime == nullptr || !_sessionState || !_sessionState->isActive()) {
    return false;
  }
  std::lock_guard<std::mutex> mailboxLock(_mailboxMutex);
  _mailbox.push_back(std::move(job));
  return true;
}

void RuntimeContext::onTaskSettled(bool keepPumping) noexcept {
  if (!keepPumping) {
    return;
  }

  auto count = _pumpTasks.load(std::memory_order_acquire);
  while (count > 0 && !_pumpTasks.compare_exchange_weak(
                          count, count - 1, std::memory_order_acq_rel)) {
  }
}

bool RuntimeContext::registerTask(const void *id, std::shared_ptr<void> owner,
                                  std::function<void()> cancel) {
  if (id == nullptr || !owner || !cancel) {
    return false;
  }

  std::lock_guard<std::recursive_mutex> lifecycleLock(_lifecycleMutex);
  if (_runtime == nullptr || !_sessionState || !_sessionState->isActive()) {
    return false;
  }
  std::lock_guard<std::mutex> tasksLock(_tasksMutex);
  _activeTasks.insert_or_assign(
      id, ActiveTask{std::move(owner), std::move(cancel)});
  return true;
}

void RuntimeContext::unregisterTask(const void *id) noexcept {
  if (id == nullptr) {
    return;
  }

  std::lock_guard<std::mutex> lock(_tasksMutex);
  _activeTasks.erase(id);
}

RuntimeContext::InvalidationCallbackId
RuntimeContext::addInvalidationCallback(std::function<void()> callback) {
  if (!callback) {
    return 0;
  }

  std::lock_guard<std::recursive_mutex> lifecycleLock(_lifecycleMutex);
  if (_runtime == nullptr || !_sessionState || !_sessionState->isActive()) {
    return 0;
  }
  std::lock_guard<std::mutex> callbacksLock(_invalidationCallbacksMutex);
  const auto id = _nextInvalidationCallbackId++;
  if (id == 0) {
    return 0;
  }
  _invalidationCallbacks.emplace(id, std::move(callback));
  return id;
}

void RuntimeContext::removeInvalidationCallback(
    InvalidationCallbackId id) noexcept {
  if (id == 0) {
    return;
  }

  const auto invoker = callInvoker();
  if (!invoker) {
    // Keep the callback registered. Runtime invalidation is the only remaining
    // point where JSI-owned state can be destroyed on its owning thread.
    return;
  }

  const auto weakContext = weak_from_this();
  try {
    invoker->invokeAsync([weakContext, id](jsi::Runtime &runtime) noexcept {
      if (const auto context = weakContext.lock();
          context && context->ownsRuntime(runtime)) {
        context->executeAndRemoveInvalidationCallback(id);
      }
    });
  } catch (...) {
    // The callback stays registered and will be executed by invalidate().
  }
}

void RuntimeContext::executeAndRemoveInvalidationCallback(
    InvalidationCallbackId id) noexcept {
  std::function<void()> callback;
  {
    std::lock_guard<std::mutex> lock(_invalidationCallbacksMutex);
    const auto callbackIt = _invalidationCallbacks.find(id);
    if (callbackIt == _invalidationCallbacks.end()) {
      return;
    }
    callback = std::move(callbackIt->second);
    _invalidationCallbacks.erase(callbackIt);
  }

  if (!callback) {
    return;
  }
  try {
    callback();
  } catch (...) {
    // Cleanup dispatched from a noexcept destructor path must not escape.
  }
}

void RuntimeContext::requestTick() {
  jsi::Runtime *runtime = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
    if (_runtime == nullptr || !_instance) {
      return;
    }
    if (!_sessionState || !_sessionState->isActive()) {
      throw std::runtime_error("WebGPU runtime session is no longer active");
    }
    runtime = _runtime;
  }

  bool expected = false;
  if (!_tickScheduled.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel)) {
    return;
  }

  try {
    auto self = shared_from_this();
    jsi::Runtime &owningRuntime = *runtime;
    auto callback = jsi::Function::createFromHostFunction(
        owningRuntime,
        jsi::PropNameID::forAscii(owningRuntime, "RNWGPUAsyncTick"), 0,
        [self](jsi::Runtime & /*runtime*/, const jsi::Value & /*thisValue*/,
               const jsi::Value * /*arguments*/,
               size_t /*count*/) -> jsi::Value {
          self->tick();
          return jsi::Value::undefined();
        });

    auto global = owningRuntime.global();
    auto setTimeout = global.getProperty(owningRuntime, "setTimeout");
    if (setTimeout.isObject() &&
        setTimeout.getObject(owningRuntime).isFunction(owningRuntime)) {
      setTimeout.getObject(owningRuntime).getFunction(owningRuntime).call(
          owningRuntime, jsi::Value(owningRuntime, callback), jsi::Value(0.0));
      return;
    }

    auto setImmediate = global.getProperty(owningRuntime, "setImmediate");
    if (setImmediate.isObject() &&
        setImmediate.getObject(owningRuntime).isFunction(owningRuntime)) {
      setImmediate.getObject(owningRuntime).getFunction(owningRuntime).call(
          owningRuntime, jsi::Value(owningRuntime, callback));
      return;
    }
    owningRuntime.queueMicrotask(std::move(callback));
  } catch (...) {
    _tickScheduled.store(false, std::memory_order_release);
    throw;
  }
}

void RuntimeContext::tick() noexcept {
  wgpu::Instance instance;
  bool sessionExpired = false;
  {
    std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
    if (_runtime == nullptr) {
      return;
    }
    _tickScheduled.store(false, std::memory_order_release);
    sessionExpired = !_sessionState || !_sessionState->isActive();
    if (sessionExpired) {
      instance = nullptr;
    } else {
      instance = _instance;
    }
  }

  if (sessionExpired) {
    // Timer callbacks execute on this context's owning runtime thread, where
    // releasing pending JSI values is safe.
    invalidateForReplacement();
    return;
  }

  try {
    std::lock_guard<std::mutex> lock(processEventsMutex());
    instance.ProcessEvents();
  } catch (...) {
    invalidate();
    return;
  }

  drainMailbox();
  if (!isValid()) {
    // The session may have expired while ProcessEvents was running. This tick
    // is already on the owning runtime thread, so finish JSI cleanup instead
    // of silently stopping the pump with tracked Promises still alive.
    invalidateForReplacement();
    return;
  }
  if (_pumpTasks.load(std::memory_order_acquire) > 0) {
    try {
      requestTick();
    } catch (...) {
      invalidate();
    }
  }
}

void RuntimeContext::drainMailbox() noexcept {
  std::vector<std::function<void()>> jobs;
  {
    std::lock_guard<std::mutex> lock(_mailboxMutex);
    jobs.swap(_mailbox);
  }
  for (auto &job : jobs) {
    try {
      job();
    } catch (...) {
      // Timer callbacks must not propagate through the JSI HostFunction.
    }
  }
}

void RuntimeContext::detachMainRuntime(RNWebGPUSessionId sessionId) noexcept {
  std::lock_guard<std::recursive_mutex> lock(_lifecycleMutex);
  if (_mainSessionId != sessionId) {
    return;
  }
  _mainSessionId = kInvalidRNWebGPUSessionId;
  _callInvoker.reset();
}

jsi::UUID RuntimeContext::runtimeDataUUID() {
  static constexpr jsi::UUID uuid(0xC06D5D2D, 0xB19B, 0x431B, 0xA89E,
                                  0x20F8A0B6E31BULL);
  return uuid;
}

} // namespace rnwgpu::async
