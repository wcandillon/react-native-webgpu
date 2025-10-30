#pragma once

#ifdef __APPLE__

#include <dispatch/dispatch.h>
#include <functional>
#include <pthread.h>

namespace rnwgpu {

namespace detail {
inline void RunOnMainThreadSyncInvoke(void *context) {
  auto *funcPtr = static_cast<std::function<void()> *>(context);
  (*funcPtr)();
}
} // namespace detail

inline void RunOnMainThreadSync(const std::function<void()> &task) {
  if (!task) {
    return;
  }

  if (pthread_main_np() != 0) {
    task();
    return;
  }

  auto callable = task;
  dispatch_sync_f(dispatch_get_main_queue(), &callable,
                  detail::RunOnMainThreadSyncInvoke);
}

} // namespace rnwgpu

#endif
