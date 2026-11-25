#pragma once

#include <chrono>

namespace rnwgpu {

// purgeGpuResources(), which the system also brings about when it is short of
// memory and, on Android, when the app goes to the background: Dawn gives the
// GPU memory it keeps for reuse (the memory of destroyed textures and buffers,
// staging memory) back to the system, for every live device (see
// GPUDevice::reduceMemoryUsage). Nothing alive is affected.
//
// The platform modules only report what the system said (onMemoryWarning,
// onLowMemory, onTrimMemory): whether it calls for a purge is decided here.
//
// request() returns at once. The work runs on a thread of its own, started by
// a request and gone once done; a request made while it runs is served by one
// more run of the same thread, however many came in.
class GpuPurge {
public:
  // purgeGpuResources() from JS. Any thread.
  static void request() noexcept;

  // iOS, UIApplicationDidReceiveMemoryWarningNotification: the system is short
  // of memory, and ends the apps that hold the most of it first. Any thread.
  static void onMemoryWarning() noexcept;

  // Android, ComponentCallbacks.onLowMemory(): the whole system is short of
  // memory. Any thread.
  static void onLowMemory() noexcept;

  // Android, ComponentCallbacks2.onTrimMemory(): purges from
  // kTrimMemoryUiHidden, which is each time the app goes to the background.
  // Any thread.
  static void onTrimMemory(int level) noexcept;

private:
  // One run asks Dawn again for as long as it says deletions wait for
  // submitted work to complete, this many times at most and this far apart:
  // about a second in all.
  static constexpr int kMaxPasses = 10;
  static constexpr std::chrono::milliseconds kPassInterval{100};

  // The passes of a run that also ask the devices only the thread of their
  // runtime may use (see GPUDevice::reduceMemoryUsageOnRuntimeThreads). What
  // Dawn answers there is not waited for, hence a fixed number: the first lets
  // the pending deletions go through, the next free what they leave.
  static constexpr int kRuntimeThreadPasses = 3;

  // ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN: the app is no longer visible. It
  // renders nothing there, and the system ends the processes in the background
  // that hold the most memory first. The system sends it each time the app
  // leaves the foreground, and it may be all it sends: TRIM_MEMORY_BACKGROUND
  // did not come to an app left in the background until it was cached and
  // frozen (a Samsung on Android 17). The levels below this one are for an app
  // in the foreground, and are no longer sent since Android 14.
  static constexpr int kTrimMemoryUiHidden = 20;

  // The purge thread: runs until no request is left.
  static void runRequests() noexcept;

  // One run: the passes over the live devices.
  static void run();
};

} // namespace rnwgpu
