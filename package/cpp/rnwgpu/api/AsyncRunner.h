#pragma once

#include "webgpu/webgpu_cpp.h"

#include <future>
#include <utility>

namespace rnwgpu {

class AsyncRunner {
public:
  explicit AsyncRunner(wgpu::Instance *instance) : instance(instance) {}

  auto runAsync(std::function<wgpu::Future()> func) -> std::future<void> {
    return std::async(std::launch::async, [this, func = std::move(func)]() {
      auto future = func();
      instance->WaitAny(future, UINT64_MAX);
    });
  }

private:
  wgpu::Instance *instance;
};

} // namespace rnwgpu
