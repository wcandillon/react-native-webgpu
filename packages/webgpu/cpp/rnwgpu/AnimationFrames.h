#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
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
class AnimationFrameSource {
public:
  // The time of the frame (when the frame of the display started), in
  // nanoseconds of std::chrono::steady_clock.
  using FrameCallback = std::function<void(int64_t frameTimeNanos)>;

  virtual ~AnimationFrameSource() = default;

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
 * Process-wide: the animation frame source is the platform's and outlives a
 * reload. The per-runtime state (RuntimeAnimationFrames) lives in the
 * runtime's runtimeData, and the dispatcher only holds weak references to it.
 */
class AnimationFrameDispatcher {
public:
  struct AnimationFrame {
    uint64_t id = 0;
    int64_t timeNanos = 0;
  };

  static AnimationFrameDispatcher &shared();

  // Installed by the platform on install (RNWebGPUManager); without one,
  // every runtime falls back to its own global requestAnimationFrame.
  void setSource(std::shared_ptr<AnimationFrameSource> source);
  bool hasSource();

  // Whether the app is in the foreground: the host activity is resumed
  // (Android), the app is active (iOS). While it is not, the source is asked
  // for no frame, and what waits for one gets the first frame after the app
  // is back: a render loop stops with the app. Set by the platform, as
  // neither source stops on its own; true where the platform says nothing.
  // Any thread.
  void setForeground(bool foreground);

  // The latest frame of the display that started.
  AnimationFrame latestFrame();

  // The latest frame, when the caller is still within it (it started less
  // than a frame interval ago) and it is newer than `after`.
  bool currentFrameAfter(uint64_t after, AnimationFrame &frame);

  // Has `frames` delivered the next frame, with the request it answers
  // (RuntimeAnimationFrames::deliver, on the source's thread). Any thread.
  void requestDelivery(const std::shared_ptr<RuntimeAnimationFrames> &frames,
                       uint64_t request);

  // Blocks until a frame newer than `after` starts, or `timeoutNanos` passes.
  // Returns false on timeout (the display sends no frames: the app is in the
  // background, say). The frame the caller is still within counts.
  bool waitForFrameAfter(uint64_t after, int64_t timeoutNanos,
                         AnimationFrame &frame);

  static int64_t nowNanos();

private:
  AnimationFrameDispatcher() = default;

  // A runtime that waits for the next frame, and the request it waits with.
  struct Delivery {
    std::weak_ptr<RuntimeAnimationFrames> frames;
    uint64_t request;
  };

  void onFrame(int64_t frameTimeNanos);
  // Called with _mutex held.
  void requestFrameLocked();

  std::mutex _mutex;
  std::condition_variable _frameStarted;
  std::shared_ptr<AnimationFrameSource> _source;
  AnimationFrame _latest;
  // The display's frame interval, from consecutive frames: 60 Hz until known.
  int64_t _intervalNanos = 16'666'667;
  bool _frameRequested = false;
  bool _foreground = true;
  // Frames delivered with nothing waiting: the source keeps ticking a few
  // frames after the last request, so a loop that overruns a frame finds the
  // frame it overran into (see currentFrameAfter) rather than waiting for the
  // one after.
  int _idleFrames = 0;
  int _waiters = 0;
  std::vector<Delivery> _pending;
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
    // runtime (worklets' is paced by the display already), and any runtime
    // when no animation frame source is installed.
    Delegated,
  };

  RuntimeAnimationFrames(jsi::Runtime &runtime, Kind kind);

  static std::shared_ptr<RuntimeAnimationFrames>
  forRuntime(jsi::Runtime &runtime);

  // Runtime thread.
  double request(jsi::Runtime &runtime, jsi::Function callback);
  void cancel(double handle);

  // The source's thread: the frame asked for with requestDelivery started.
  void deliver(const AnimationFrameDispatcher::AnimationFrame &frame,
               uint64_t request);

private:
  // How a run of the callbacks comes about, on the main runtime.
  enum class Run {
    // None yet.
    kNone,
    // On an animation frame: posted as the frame started, while no run was
    // going on. It goes ahead of what waits on the runtime, like React
    // Native's timers.
    kOnFrame,
    // In a frame that had started already: the one a loop overran into. It
    // goes behind what waits on the runtime, and only after a run on an
    // animation frame, so that two in a row at most are back to back.
    kInFrame,
  };

  void schedule(jsi::Runtime &runtime);
  // Posts the run of `request` in `frame` to the main runtime. Any thread.
  void post(const AnimationFrameDispatcher::AnimationFrame &frame,
            uint64_t request, Run run);
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
  uint64_t _requests = 0;
  // The main runtime, read on the source's thread too (see deliver). What the
  // callbacks wait on: a number per schedule(), 0 once they ran. A run posted
  // for another request finds it answered, and does nothing.
  std::atomic<uint64_t> _request{0};
  // The request a run in its frame was posted for: one each.
  std::atomic<uint64_t> _inFrameRequest{0};
  // Whether the callbacks are running, and how the latest run came about.
  std::atomic<bool> _running{false};
  std::atomic<Run> _lastRun{Run::kNone};
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
