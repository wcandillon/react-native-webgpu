#include "CallInvokerScheduler.h"

#include <memory>
#include <utility>

namespace rnwgpu::async {

CallInvokerScheduler::CallInvokerScheduler(
    std::shared_ptr<react::CallInvoker> invoker)
    : _invoker(std::move(invoker)) {}

void CallInvokerScheduler::scheduleOnJS(
    std::function<void(jsi::Runtime &)> job) {
  if (!_invoker || !job) {
    return;
  }
  _invoker->invokeAsync(
      [job = std::move(job)](jsi::Runtime &runtime) { job(runtime); });
}

} // namespace rnwgpu::async
