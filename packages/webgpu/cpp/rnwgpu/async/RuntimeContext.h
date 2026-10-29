#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <jsi/jsi.h>

#include "AsyncTaskHandle.h"

#include "webgpu/webgpu_cpp.h"

namespace jsi = facebook::jsi;

namespace facebook::react {
class CallInvoker;
} // namespace facebook::react

namespace rnwgpu::async {

/**
 * How asynchronous WebGPU operations get back to the JS runtime that
 * requested them. One per (JS runtime, Dawn instance), made on first use and
 * kept in the runtime's runtimeData, so that it lives exactly as long as the
 * runtime. A runtime usually has one, for the shared WebGPURuntime instance;
 * a device passed to importDevice() can live on another instance, which then
 * gets a context of its own.
 *
 * Waiting: Dawn operations are registered with CallbackMode::WaitAnyOnly, and
 * their futures handed to awaitFuture(). One already complete runs its
 * callback right there; the others go to a thread per instance that sleeps
 * in Instance::WaitAny until the GPU (or Dawn's worker) is done, then runs
 * the callback. Nothing polls meanwhile: neither the JS thread nor anyone
 * else calls ProcessEvents.
 *
 * Settling: a callback runs on the waiter thread (or wherever a non-Dawn
 * operation finishes, see GPUQueue::drawElementImageToTexture), so it never
 * touches JSI; it hands a settle action to post(), which runs it on the
 * runtime's own thread. How depends on the runtime:
 *
 * - The main React Native runtime: its CallInvoker (registerMainRuntime).
 * - The Reanimated UI runtime: through the main runtime, where
 *   `__webgpuScheduleOnUI` (installed by registerWebGPUForReanimated) hands
 *   a native function to worklets' scheduleOnUI, which runs it on the UI
 *   runtime.
 * - Any other worklet runtime (createWorkletRuntime, a Vision Camera frame
 *   processor): worklets offers nothing to post to it from native, but the
 *   runtime has a setTimeout. Actions go into a mailbox, and a timer armed
 *   on the runtime itself when an operation starts (postTask runs on that
 *   thread) drains the mailbox, re-arming itself while an operation is
 *   outstanding. The runtime polls its mailbox meanwhile, which lasts only
 *   as long as the operations do; a spontaneous event (device.lost, an
 *   uncaptured error) posted while nothing is outstanding is delivered with
 *   the next operation.
 *
 * Threading contract: postTask() runs on the runtime's thread (it is the
 * entry point of every async method, called with the calling runtime, see
 * GPUBuffer::mapAsync); awaitFuture() and post() run on any thread.
 */
class RuntimeContext : public std::enable_shared_from_this<RuntimeContext> {
public:
  using TaskCallback = std::function<std::optional<wgpu::Future>(
      const AsyncTaskHandle::ResolveFunction &,
      const AsyncTaskHandle::RejectFunction &)>;
  using Job = std::function<void(jsi::Runtime &)>;

  RuntimeContext(jsi::Runtime &runtime, wgpu::Instance instance);

  // The calling runtime's context for `instance`, created on first use and
  // kept in the runtime's runtimeData for the runtime's lifetime.
  static std::shared_ptr<RuntimeContext>
  getOrCreate(jsi::Runtime &runtime, const wgpu::Instance &instance);

  // Register the main JS runtime and its CallInvoker: how the main runtime
  // is reached, and the way to the UI runtime. Called once from
  // RNWebGPUManager on install.
  static void
  registerMainRuntime(jsi::Runtime *runtime,
                      std::shared_ptr<facebook::react::CallInvoker> invoker);

  // The wgpu::Instance served by this context on its owning runtime.
  const wgpu::Instance &instance() const { return _instance; }

  // The runtime this context belongs to. Safe for pointer-identity checks from
  // any thread; only touch JSI through it on the runtime's own thread.
  jsi::Runtime &runtime() const { return _runtime; }

  // Starts an asynchronous operation whose promise settles on this runtime:
  // `callback` gets the resolve and reject functions, starts the work, and
  // returns the Dawn future to wait for, if the work is Dawn's (it is awaited
  // like awaitFuture does). Called on the runtime's thread.
  AsyncTaskHandle postTask(const TaskCallback &callback);

  // Has the callback of `future` run: now, when it is complete already, else
  // on the instance's waiter thread once it is. Any thread.
  void awaitFuture(wgpu::Future future);

  // Runs `job` on this runtime's thread, later, with the runtime. Any thread.
  void post(Job job);

  // Invoked by a settle action once it has run, on the runtime's thread.
  void onTaskSettled();

private:
  enum class Kind { Main, UI, Mailbox };

  static jsi::UUID runtimeDataUUID();
  static Kind kindOf(jsi::Runtime &runtime);

  void armTick(jsi::Runtime &runtime);
  void drainMailbox(jsi::Runtime &runtime);

  jsi::Runtime &_runtime;
  wgpu::Instance _instance;
  const Kind _kind;
  // The mailbox and its timer (Kind::Mailbox only).
  std::atomic<std::size_t> _outstanding{0};
  std::atomic<bool> _tickArmed{false};
  std::mutex _mailboxMutex;
  std::vector<Job> _mailbox;
};

} // namespace rnwgpu::async
