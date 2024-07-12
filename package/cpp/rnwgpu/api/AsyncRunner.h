#pragma once

#include "webgpu/webgpu_cpp.h"

#include <future>
#include <utility>

namespace rnwgpu {

class AsyncRunner {
public:
  explicit AsyncRunner(wgpu::Instance *instance) : instance(instance) {}

  template <typename Func>
  auto runAsync(Func &&func) -> std::future<std::invoke_result_t<Func>> {
    using ReturnType = std::invoke_result_t<Func>;
    return std::async(std::launch::async,
                      [this, func = std::forward<Func>(func)]() -> ReturnType {
                        if constexpr (std::is_void_v<ReturnType>) {
                          func();
                          instance->ProcessEvents();
                        } else {
                          auto result = func();
                          instance->ProcessEvents();
                          return result;
                        }
                      });
  }

private:
  wgpu::Instance *instance;
};

} // namespace rnwgpu
