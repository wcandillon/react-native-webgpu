#pragma once

#include "webgpu/webgpu_cpp.h"

#include <future>
#include <utility>

namespace rnwgpu {

class AsyncRunner {
public:
  explicit AsyncRunner(wgpu::Instance *instance) : instance(instance) {}

  template <typename F> auto runAsync(F &&func) {
    return std::async(
        std::launch::async, [this, func = std::forward<F>(func)]() {
          if constexpr (std::is_invocable_v<F, wgpu::Instance *>) {
            return func(instance);
          } else {
            auto future = func();
            instance->WaitAny(future, UINT64_MAX);
          }
        });
  }

private:
  wgpu::Instance *instance;
};

} // namespace rnwgpu
