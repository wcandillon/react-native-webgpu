
#pragma once

#include <utility>
#include <memory>
#include <ReactCommon/CallInvoker.h>

namespace rnwgpu {

/**
 * A Dispatcher that uses react::CallInvoker for it's implementation
 */
class CallInvokerDispatcher {
public:
  explicit CallInvokerDispatcher(
      std::shared_ptr<facebook::react::CallInvoker> callInvoker)
      : _callInvoker(callInvoker) {}

  void runAsync(std::function<void()> &&function) {
    _callInvoker->invokeAsync(std::move(function));
  }

  void runSync(std::function<void()> &&function) {
    _callInvoker->invokeSync(std::move(function));
  }

public:
  std::shared_ptr<facebook::react::CallInvoker> _callInvoker;
};

} // namespace rnwgpu
