#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "AsyncTaskHandle.h"
#include "RNWebGPUSession.h"
#include "webgpu/webgpu_cpp.h"

namespace jsi = facebook::jsi;

namespace facebook::react {
class CallInvoker;
} // namespace facebook::react

namespace rnwgpu::async {

/**
 * Runtime-owned coordinator for asynchronous WebGPU operations.
 *
 * One context is stored in each JSI runtime's runtimeData. The runtimeData
 * destructor invalidates the context before the runtime disappears, cancels
 * every pending task, and clears all queued settle actions. Dawn callbacks
 * retain only weak task/context references, so callbacks arriving later are
 * harmless no-ops.
 *
 * Request/response operations always use the CALLING runtime's context. This
 * preserves the cross-runtime/boxed-object contract: a Promise is settled on
 * the same runtime that created it, not necessarily the runtime that created
 * the WebGPU native object.
 */
class RuntimeContext final
    : public std::enable_shared_from_this<RuntimeContext> {
public:
  using TaskCallback =
      std::function<void(const AsyncTaskHandle::ResolveFunction &,
                         const AsyncTaskHandle::RejectFunction &)>;

  RuntimeContext(jsi::Runtime &runtime, wgpu::Instance instance,
                 std::shared_ptr<RNWebGPUSessionState> sessionState);
  ~RuntimeContext();

  RuntimeContext(const RuntimeContext &) = delete;
  RuntimeContext &operator=(const RuntimeContext &) = delete;
  RuntimeContext(RuntimeContext &&) = delete;
  RuntimeContext &operator=(RuntimeContext &&) = delete;

  static std::shared_ptr<RuntimeContext> get(jsi::Runtime &runtime);
  static std::shared_ptr<RuntimeContext>
  getOrCreate(jsi::Runtime &runtime, wgpu::Instance instance,
              std::shared_ptr<RNWebGPUSessionState> sessionState);

  static void
  registerMainRuntime(RNWebGPUSessionId sessionId, jsi::Runtime *runtime,
                      std::shared_ptr<facebook::react::CallInvoker> invoker);
  static void unregisterMainRuntime(RNWebGPUSessionId sessionId) noexcept;

  void invalidate() noexcept;
  bool isValid() const noexcept;
  bool ownsRuntime(const jsi::Runtime &runtime) const noexcept;
  const void *runtimeIdentity() const noexcept;

  /** Call only from this context's owning runtime thread. */
  bool withRuntime(const std::function<void(jsi::Runtime &)> &action);

  std::shared_ptr<facebook::react::CallInvoker> callInvoker() const noexcept;
  wgpu::Instance instance() const;
  std::shared_ptr<RNWebGPUSessionState> sessionState() const noexcept;

  AsyncTaskHandle postTask(const TaskCallback &callback,
                           bool keepPumping = true);

  bool beginPumping();
  bool postSettle(std::function<void()> job);
  void onTaskSettled(bool keepPumping) noexcept;

  /** RuntimeContext owns each task until it settles or runtimeData tears down.
   */
  bool registerTask(const void *id, std::shared_ptr<void> owner,
                    std::function<void()> cancel);
  void unregisterTask(const void *id) noexcept;

  using InvalidationCallbackId = std::uint64_t;
  InvalidationCallbackId
  addInvalidationCallback(std::function<void()> callback);

  /**
   * Schedule removal and execution of an invalidation callback on this
   * context's owning runtime thread. If dispatch is unavailable, the callback
   * remains registered and invalidate() executes it during runtime teardown.
   * Safe to call from any thread.
   */
  void removeInvalidationCallback(InvalidationCallbackId id) noexcept;

private:
  static jsi::UUID runtimeDataUUID();

  void invalidateForReplacement() noexcept;
  void invalidateImpl(bool purgeRegistrations) noexcept;
  void requestTick();
  void tick() noexcept;
  void drainMailbox() noexcept;
  void detachMainRuntime(RNWebGPUSessionId sessionId) noexcept;
  void executeAndRemoveInvalidationCallback(InvalidationCallbackId id) noexcept;

  mutable std::recursive_mutex _lifecycleMutex;
  jsi::Runtime *_runtime;
  wgpu::Instance _instance;
  std::shared_ptr<RNWebGPUSessionState> _sessionState;
  RNWebGPUSessionId _mainSessionId{kInvalidRNWebGPUSessionId};
  std::shared_ptr<facebook::react::CallInvoker> _callInvoker;
  std::atomic<std::size_t> _pumpTasks{0};
  std::atomic<bool> _tickScheduled{false};

  std::mutex _mailboxMutex;
  std::vector<std::function<void()>> _mailbox;

  struct ActiveTask final {
    std::shared_ptr<void> owner;
    std::function<void()> cancel;
  };
  std::mutex _tasksMutex;
  std::unordered_map<const void *, ActiveTask> _activeTasks;

  std::mutex _invalidationCallbacksMutex;
  std::unordered_map<InvalidationCallbackId, std::function<void()>>
      _invalidationCallbacks;
  InvalidationCallbackId _nextInvalidationCallbackId{1};
};

} // namespace rnwgpu::async
