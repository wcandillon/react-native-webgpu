#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <jsi/jsi.h>

namespace rnwgpu {

namespace jsi = facebook::jsi;

/**
 * The display's frames, from the platform: an AChoreographer on a looper
 * thread of its own (Android), a CADisplayLink on a run loop thread of its own
 * (iOS). One-shot like requestAnimationFrame: requestFrame() has the frame
 * callback called once, at the next frame of the display, on the source's
 * thread. Requests made before that frame are coalesced.
 */
class VsyncSource {
public:
  // The time of the frame (when the display's vsync started it), in
  // nanoseconds of std::chrono::steady_clock.
  using FrameCallback = std::function<void(int64_t frameTimeNanos)>;

  virtual ~VsyncSource() = default;

  // Set once, before the first requestFrame().
  void setFrameCallback(FrameCallback callback) {
    _frameCallback = std::move(callback);
  }

  // Any thread.
  virtual void requestFrame() = 0;

protected:
  void deliverFrame(int64_t frameTimeNanos) {
    if (_frameCallback) {
      _frameCallback(frameTimeNanos);
    }
  }

private:
  FrameCallback _frameCallback;
};

class RuntimeAnimationFrames;

/**
 * requestAnimationFrame for every runtime WebGPU renders from, paced by the
 * display rather than by React Native's timers. See AnimationFrames.cpp for
 * how each kind of runtime is reached, and src/animationFrame.ts for the API.
 *
 * Process-wide: the vsync source is the platform's and outlives a reload. The
 * per-runtime state (RuntimeAnimationFrames) lives in the runtime's
 * runtimeData, and the scheduler only holds weak references to it.
 */
class AnimationFrameScheduler {
public:
  struct Frame {
    uint64_t id = 0;
    int64_t timeNanos = 0;
  };

  static AnimationFrameScheduler &shared();

  // Installed by the platform on install (RNWebGPUManager); without one,
  // every runtime falls back to its own global requestAnimationFrame.
  void setVsyncSource(std::shared_ptr<VsyncSource> source);
  bool hasVsyncSource();

  // The latest frame, when the caller is still within it (it started less
  // than a frame interval ago) and it is newer than `after`.
  bool currentFrameAfter(uint64_t after, Frame &frame);

  // Has `frames` delivered the next frame (RuntimeAnimationFrames::deliver,
  // on the vsync thread). Any thread.
  void requestDelivery(const std::shared_ptr<RuntimeAnimationFrames> &frames);

  // Blocks until a frame newer than `after` starts, or `timeoutNanos` passes.
  // Returns false on timeout (the display sends no frames: the app is in the
  // background, say). The frame the caller is still within counts.
  bool waitForFrameAfter(uint64_t after, int64_t timeoutNanos, Frame &frame);

  static int64_t nowNanos();

private:
  AnimationFrameScheduler() = default;

  void onFrame(int64_t frameTimeNanos);
  // Called with _mutex held.
  void requestFrameLocked();

  std::mutex _mutex;
  std::condition_variable _frameStarted;
  std::shared_ptr<VsyncSource> _source;
  Frame _latest;
  // The display's frame interval, from consecutive frames: 60 Hz until known.
  int64_t _intervalNanos = 16'666'667;
  bool _frameRequested = false;
  // Frames delivered with nothing waiting: the source keeps ticking a few
  // frames after the last request, so a loop that overruns a frame finds the
  // frame it overran into (see currentFrameAfter) rather than waiting for the
  // one after.
  int _idleFrames = 0;
  int _waiters = 0;
  std::vector<std::weak_ptr<RuntimeAnimationFrames>> _pending;
};

/**
 * The animation frame callbacks of one runtime. Owned by the runtime (its
 * runtimeData); the callbacks are only touched on the runtime's thread.
 */
class RuntimeAnimationFrames
    : public std::enable_shared_from_this<RuntimeAnimationFrames> {
public:
  enum class Kind {
    // The React Native runtime: frames are posted through its CallInvoker.
    Main,
    // A worklet runtime of the app (createWorkletRuntime, a frame processor):
    // its setTimeout wakes it, and it waits for the frame on its thread.
    Waiting,
    // The runtime's own requestAnimationFrame paces it: the Reanimated UI
    // runtime (worklets' is vsync-aligned already), and any runtime when no
    // vsync source is installed.
    Delegated,
  };

  RuntimeAnimationFrames(jsi::Runtime &runtime, Kind kind);

  static std::shared_ptr<RuntimeAnimationFrames>
  forRuntime(jsi::Runtime &runtime);

  // Runtime thread.
  double request(jsi::Runtime &runtime, jsi::Function callback);
  void cancel(double handle);

  // Vsync thread: the frame requested through requestDelivery started.
  void deliver(const AnimationFrameScheduler::Frame &frame);

private:
  void schedule(jsi::Runtime &runtime);
  void scheduleDelegated(jsi::Runtime &runtime);
  void scheduleWaiting(jsi::Runtime &runtime, double delayMs);
  void flush(jsi::Runtime &runtime, uint64_t frameId, double timestampMs);
  static double timestampOf(jsi::Runtime &runtime, int64_t frameTimeNanos);

  jsi::Runtime &_runtime;
  const Kind _kind;
  // Runtime thread only.
  std::map<double, std::shared_ptr<jsi::Function>> _callbacks;
  std::map<double, std::shared_ptr<jsi::Function>> *_flushing = nullptr;
  double _nextHandle = 1;
  bool _scheduled = false;
  // The last frame run here.
  std::atomic<uint64_t> _lastFrameId{0};
};

/**
 * The JS face of the above, installed on the main runtime as
 * `__webgpuAnimationFrames` and handed to other runtimes by reference (a
 * HostObject, like WebGPUWorkletHelpers): request(callback) and
 * cancel(handle) act on the runtime that calls them.
 */
class AnimationFramesHostObject : public jsi::HostObject {
public:
  jsi::Value get(jsi::Runtime &runtime, const jsi::PropNameID &name) override;
  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &runtime) override;
};

} // namespace rnwgpu
