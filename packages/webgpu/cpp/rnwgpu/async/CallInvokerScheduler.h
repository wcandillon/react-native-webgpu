#pragma once

#include <functional>
#include <memory>

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>

#include "RuntimeScheduler.h"

namespace rnwgpu::async {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

/**
 * RuntimeScheduler for the main React Native JS runtime, backed by
 * react::CallInvoker::invokeAsync. invokeAsync is safe to call from any thread
 * and delivers the work on the JS thread with the runtime, which is exactly the
 * contract RuntimeScheduler requires.
 */
class CallInvokerScheduler final : public RuntimeScheduler {
public:
  explicit CallInvokerScheduler(std::shared_ptr<react::CallInvoker> invoker);

  void scheduleOnJS(std::function<void(jsi::Runtime &)> job) override;

private:
  std::shared_ptr<react::CallInvoker> _invoker;
};

} // namespace rnwgpu::async
