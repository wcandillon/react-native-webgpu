#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <jsi/jsi.h>

#include "AsyncDispatcher.h"
#include "AsyncTaskHandle.h"

#include "webgpu/webgpu_cpp.h"

namespace jsi = facebook::jsi;

namespace rnwgpu::async {

class AsyncRunner : public std::enable_shared_from_this<AsyncRunner> {
public:
  using TaskCallback = std::function<void(const AsyncTaskHandle::ResolveFunction&, const AsyncTaskHandle::RejectFunction&)>;

  AsyncRunner(wgpu::Instance instance, std::shared_ptr<AsyncDispatcher> dispatcher);

  static std::shared_ptr<AsyncRunner> get(jsi::Runtime& runtime);
  static std::shared_ptr<AsyncRunner> getOrCreate(jsi::Runtime& runtime, wgpu::Instance instance,
                                                  std::shared_ptr<AsyncDispatcher> dispatcher);

  AsyncTaskHandle postTask(const TaskCallback& callback, bool keepPumping = true);

  void requestTick();
  void tick(jsi::Runtime& runtime);
  void onTaskSettled(bool keepPumping);

  std::shared_ptr<AsyncDispatcher> dispatcher() const;

private:
  static jsi::UUID runtimeDataUUID();

  wgpu::Instance _instance;
  std::shared_ptr<AsyncDispatcher> _dispatcher;
  std::atomic<size_t> _pendingTasks;
  std::atomic<size_t> _pumpTasks;
  std::atomic<bool> _tickScheduled;
};

} // namespace rnwgpu::async
