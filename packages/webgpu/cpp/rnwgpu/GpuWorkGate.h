#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "WGPULogger.h"

namespace rnwgpu {

/**
 * Whether GPU work may reach the device right now, and the work held back
 * while it may not.
 *
 * iOS refuses GPU work from an app in the background, including work still
 * queued when the app got there, and Dawn (since 157) loses the device over
 * a refused Metal command buffer. So the gate follows Apple's guidance for
 * Metal apps: when the app goes to the background (WebGPUModule.mm, on the
 * notification itself so that it is done before the app is suspended) every
 * device is drained, that is what Dawn staged is committed and the GPU is
 * waited for, and from then on everything that would commit a command buffer
 * is held, in order, and run when the app comes back. That is queue.submit,
 * writeBuffer and writeTexture (Dawn commits their staging past 16 MB), the
 * internal blits, and presenting a canvas (Dawn commits the pending command
 * buffer along with the drawable). Dawn's ProcessEvents ticks every device,
 * which commits pending commands too, so the async pump (RuntimeContext)
 * idles meanwhile.
 *
 * Only a platform that refuses GPU work suspends the gate; elsewhere it stays
 * open and run() costs a lock. Any thread; the work itself runs outside of
 * the gate's lock, so it may take other locks (a surface's, Dawn's).
 */
class GpuWorkGate {
public:
  static GpuWorkGate &getInstance() {
    // Never destroyed: work may be run during process teardown.
    static auto *instance = new GpuWorkGate();
    return *instance;
  }

  GpuWorkGate(const GpuWorkGate &) = delete;
  GpuWorkGate &operator=(const GpuWorkGate &) = delete;

  /** Whether the app is in the background and GPU work is held. */
  bool isSuspended() const {
    return _suspended.load(std::memory_order_acquire);
  }

  /**
   Runs `work` now, or holds it for resume() while suspended. The work must
   own what it uses by then (the held copy is the same closure).
   */
  void run(std::function<void()> work) {
    runOrHold(work, [&work]() { return std::move(work); });
  }

  /**
   Runs `work` now, or holds the closure `hold` makes while suspended. For
   work pointing at memory it does not own (the bytes of a JS ArrayBuffer):
   `hold` copies them into a closure of its own, and is only called while
   suspended, so the common path copies nothing.
   */
  template <typename Work, typename Hold>
  void runOrHold(Work &&work, Hold &&hold) {
    {
      std::unique_lock<std::mutex> lock(_mutex);
      if (_suspended.load(std::memory_order_relaxed)) {
        _held.push_back(hold());
        return;
      }
      _active++;
    }
    // Counted while it runs, so that suspend() waits for it before draining
    // (the count is released on an exception too).
    struct Running {
      GpuWorkGate &gate;
      ~Running() {
        std::lock_guard<std::mutex> lock(gate._mutex);
        if (--gate._active == 0) {
          gate._idle.notify_all();
        }
      }
    } running{*this};
    work();
  }

  /**
   A device whose queue suspend() drains: every device the API wraps
   (GPUDevice tracks itself), imported ones included. Untracked when the
   wrapper goes.
   */
  void trackDevice(const wgpu::Device &device) {
    std::lock_guard<std::mutex> lock(_mutex);
    _devices[device.Get()] = device;
  }

  void untrackDevice(WGPUDevice handle) {
    std::lock_guard<std::mutex> lock(_mutex);
    _devices.erase(handle);
  }

  /**
   The app is going to the background: holds the work from here on, lets the
   work being run go through, then drains every device (see drain). Called
   on the thread the app state notification arrives on, and returns before
   the app is suspended: a drain is a frame's worth of GPU work.
   */
  void suspend() {
    std::vector<wgpu::Device> devices;
    {
      std::unique_lock<std::mutex> lock(_mutex);
      if (_suspended.exchange(true, std::memory_order_acq_rel)) {
        return;
      }
      _idle.wait(lock, [this]() { return _active == 0; });
      devices.reserve(_devices.size());
      for (const auto &entry : _devices) {
        devices.push_back(entry.second);
      }
    }
    for (const auto &device : devices) {
      drain(device);
    }
  }

  /** The app is back in the foreground: runs the work held meanwhile, in order.
   */
  void resume() {
    std::vector<std::function<void()>> held;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      if (!_suspended.exchange(false, std::memory_order_acq_rel)) {
        return;
      }
      held.swap(_held);
    }
    for (auto &work : held) {
      // Through run(), so that a suspend() racing with the replay waits for
      // the work or holds it again.
      run(std::move(work));
    }
  }

private:
  GpuWorkGate() = default;

  /**
   Commits what Dawn staged on the device (writeBuffer and writeTexture data,
   lazy clears) with an empty submit and waits for the GPU to finish all of
   it, within kDrainTimeoutNs. A device that does not answer in time is left
   to its fate: with work in flight it is lost when the app is suspended, and
   its owner handles that (device.lost, Skia's recovery). A lost device
   answers right away.
   */
  static void drain(const wgpu::Device &device) {
    auto queue = device.GetQueue();
    queue.Submit(0, nullptr);
    auto future = queue.OnSubmittedWorkDone(
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
    wgpu::FutureWaitInfo wait{future};
    auto status =
        device.GetAdapter().GetInstance().WaitAny(1, &wait, kDrainTimeoutNs);
    if (status != wgpu::WaitStatus::Success) {
      Logger::logToConsole(
          "[WebGPU] GPU work was still running when the app went to the "
          "background; the device may be lost.");
    }
  }

  static constexpr uint64_t kDrainTimeoutNs = 500ull * 1000 * 1000;

  std::mutex _mutex;
  std::condition_variable _idle;
  std::atomic<bool> _suspended{false};
  // Work being run right now, which suspend() waits for.
  int _active = 0;
  // The work held while suspended, in order.
  std::vector<std::function<void()>> _held;
  std::unordered_map<WGPUDevice, wgpu::Device> _devices;
};

} // namespace rnwgpu
