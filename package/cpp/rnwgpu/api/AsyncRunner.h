#pragma once

#include "webgpu/webgpu_cpp.h"

#include <future>

namespace rnwgpu {

class AsyncRunner {
public:
  explicit AsyncRunner(wgpu::Instance instance) : instance(instance) {}

  template <typename Func> std::future<void> runAsync(Func &&func) {
    return std::async(std::launch::async,
                      [this, func = std::forward<Func>(func)]() {
                        wgpu::Future future = func();
                        instance.WaitAny(future, UINT64_MAX);
                      });
  }

private:
  wgpu::Instance instance;
};

} // namespace rnwgpu