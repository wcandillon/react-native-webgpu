#include "GpuPurge.h"

#include <mutex>
#include <system_error>
#include <thread>

#include "GPUDevice.h"

namespace rnwgpu {

namespace {

struct State {
  std::mutex mutex;
  // Whether the purge thread is alive.
  bool running = false;
  // Whether a request came in since the thread last started a run.
  bool requested = false;
};

// Never destroyed: the purge thread may still use it while the process exits.
State &getState() noexcept {
  static auto *state = new State();
  return *state;
}

} // namespace

void GpuPurge::request() noexcept {
  auto &state = getState();
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.requested = true;
    if (state.running) {
      return;
    }
    state.running = true;
  }
  try {
    std::thread([] { runRequests(); }).detach();
  } catch (const std::system_error &) {
    // No thread to be had: nothing is purged.
    std::lock_guard<std::mutex> lock(state.mutex);
    state.running = false;
  }
}

void GpuPurge::onMemoryWarning() noexcept { request(); }

void GpuPurge::onLowMemory() noexcept { request(); }

void GpuPurge::onTrimMemory(int level) noexcept {
  if (level >= kTrimMemoryUiHidden) {
    request();
  }
}

void GpuPurge::runRequests() noexcept {
  auto &state = getState();
  for (;;) {
    {
      std::lock_guard<std::mutex> lock(state.mutex);
      if (!state.requested) {
        state.running = false;
        return;
      }
      state.requested = false;
    }
    run();
  }
}

void GpuPurge::run() {
  for (int pass = 0; pass < kMaxPasses; pass++) {
    bool pending = GPUDevice::reduceMemoryUsage();
    if (pass < kRuntimeThreadPasses) {
      pending |= GPUDevice::reduceMemoryUsageOnRuntimeThreads();
    }
    if (!pending) {
      return;
    }
    std::this_thread::sleep_for(kPassInterval);
  }
}

} // namespace rnwgpu
