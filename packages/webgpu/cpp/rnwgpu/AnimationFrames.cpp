#include "AnimationFrames.h"

#include <chrono>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <ReactCommon/CallInvoker.h>

#include "rnwgpu/async/RuntimeContext.h"

/**
 * How a frame reaches each kind of runtime.
 *
 * React Native's own requestAnimationFrame is a timer (setTimeout(0)): on
 * Android it fires at a frame of the display only if it was registered before
 * that frame started, which leaves a loop that asks for its next frame at the
 * end of its work about 6 ms of the 8.3 ms of a 120 Hz frame, and drops it to
 * 60 fps past that; on iOS its display link runs at 60 Hz. Worklets'
 * requestAnimationFrame on a runtime of the app's polls every 16 ms. Here,
 * the display's frames come from an animation frame source of the platform's
 * (AnimationFrameSource), and:
 *
 * - The main runtime gets them through its CallInvoker: each frame runs every
 *   callback queued before it, with the frame's time.
 * - A worklet runtime of the app has no way in from native code (this package
 *   does not link worklets), only its own timers: its setTimeout wakes it, and
 *   the runtime then waits on its own thread for the frame to start. That
 *   wait is the rest of the current frame at most, during which nothing else
 *   runs on the runtime; it gives up (and tries again later) when the display
 *   sends no frames, in the background say.
 * - The Reanimated UI runtime keeps worklets' requestAnimationFrame, which is
 *   paced by the display already (and runs on the main thread, which a source
 *   of ours would have to hop to). So does any runtime when there is no
 *   animation frame source.
 *
 * Like a browser, a frame runs the callbacks requested before it started, and
 * a callback requested while they run gets the next frame. The rules:
 *
 * - One run per frame of the display, counted from when the run starts. A
 *   run that starts late (the runtime was busy) is the run of the frame it
 *   starts in, not of the frame it was posted for: the next request then
 *   waits for the next frame, where it would otherwise find a frame that has
 *   not run here and run a second time in the same one.
 * - A loop that overruns its frame does not wait for the frame after: when
 *   the frame it overran into is still going on, it runs in it (see
 *   currentFrameAfter). On a worklet runtime it always does, so a loop that
 *   takes 10 ms runs at 100 fps at 120 Hz, not 60.
 * - The main runtime is not the loop's alone. A run in the frame the loop
 *   overran into goes behind whatever waits there (React's updates, the
 *   app's other tasks: see post), and only follows a run on an animation
 *   frame: the run after it waits for a frame of the display, which leaves
 *   the runtime to itself until then. A loop that takes 10 ms runs twice
 *   every three frames at 120 Hz, 80 fps. With every run posted as the one
 *   before it ends, the scheduler would never run out of tasks: React would
 *   not render, and the thread would not go back to its own loop, where iOS
 *   finds React Native's timers due. React Native's timer leaves the runtime
 *   the rest of each frame.
 * - No frame while the app is not in the foreground (see setForeground): a
 *   loop stops with the app and starts again with it. Neither source stops
 *   on its own: an AChoreographer sends its frames to a paused app, and a
 *   CADisplayLink runs on in the background until the system suspends the
 *   app.
 */

namespace rnwgpu {

namespace {

constexpr const char *kPropRequest = "request";
constexpr const char *kPropCancel = "cancel";

// A frame longer than this is a stretch without frames, not an interval.
constexpr int64_t kShortestInterval = 4'000'000;
constexpr int64_t kLongestInterval = 50'000'000;
// After this many frames without a request, the source stops ticking.
constexpr int kIdleFramesBeforeStop = 4;
// How long a waiting runtime waits for a frame before it gives the thread
// back, and how long it then sleeps before it asks again.
constexpr int64_t kWaitTimeoutNanos = 100'000'000;
constexpr double kRetryDelayMs = 100;

jsi::UUID runtimeDataUUID() {
  // Fixed random UUID for this slot of runtimeData. Never the all-zero UUID,
  // which Hermes reserves (see RuntimeContext::runtimeDataUUID).
  static constexpr jsi::UUID uuid(0x5A3C9E71, 0x2D4B, 0x4F8A, 0x9C1E,
                                  0x7B6D2E4F8A13ULL);
  return uuid;
}

jsi::Function getGlobalFunction(jsi::Runtime &runtime, const char *name) {
  auto value = runtime.global().getProperty(runtime, name);
  if (!value.isObject() || !value.asObject(runtime).isFunction(runtime)) {
    throw jsi::JSError(runtime, std::string("[react-native-webgpu] ") + name +
                                    " is not available on this runtime.");
  }
  return value.asObject(runtime).asFunction(runtime);
}

} // namespace

// AnimationFrameDispatcher

AnimationFrameDispatcher &AnimationFrameDispatcher::shared() {
  // Never destroyed: the animation frame source calls into it from its own
  // thread for the life of the process.
  static auto *dispatcher = new AnimationFrameDispatcher();
  return *dispatcher;
}

int64_t AnimationFrameDispatcher::nowNanos() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void AnimationFrameDispatcher::setSource(
    std::shared_ptr<AnimationFrameSource> source) {
  if (source == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(_mutex);
  // The platform's source outlives a reload: the first one stays.
  if (_source != nullptr) {
    return;
  }
  source->setFrameCallback(
      [this](int64_t frameTimeNanos) { onFrame(frameTimeNanos); });
  _source = std::move(source);
}

bool AnimationFrameDispatcher::hasSource() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _source != nullptr;
}

void AnimationFrameDispatcher::setForeground(bool foreground) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_foreground == foreground) {
    return;
  }
  _foreground = foreground;
  // What asked for a frame while the app was away gets the next one.
  if (foreground && (!_pending.empty() || _waiters > 0)) {
    _idleFrames = 0;
    requestFrameLocked();
  }
}

AnimationFrameDispatcher::AnimationFrame
AnimationFrameDispatcher::latestFrame() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _latest;
}

bool AnimationFrameDispatcher::currentFrameAfter(uint64_t after,
                                                 AnimationFrame &frame) {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_foreground && _latest.id > after &&
      nowNanos() - _latest.timeNanos < _intervalNanos) {
    frame = _latest;
    return true;
  }
  return false;
}

void AnimationFrameDispatcher::requestDelivery(
    const std::shared_ptr<RuntimeAnimationFrames> &frames, uint64_t request) {
  std::lock_guard<std::mutex> lock(_mutex);
  _pending.push_back({frames, request});
  _idleFrames = 0;
  requestFrameLocked();
}

bool AnimationFrameDispatcher::waitForFrameAfter(uint64_t after,
                                                 int64_t timeoutNanos,
                                                 AnimationFrame &frame) {
  std::unique_lock<std::mutex> lock(_mutex);
  _idleFrames = 0;
  requestFrameLocked();
  if (_foreground && _latest.id > after &&
      nowNanos() - _latest.timeNanos < _intervalNanos) {
    // Caught up with the frame the caller is in (see currentFrameAfter).
    frame = _latest;
    return true;
  }
  const uint64_t current = _latest.id;
  _waiters++;
  const bool started =
      _frameStarted.wait_for(lock, std::chrono::nanoseconds(timeoutNanos),
                             [&]() { return _latest.id > current; });
  _waiters--;
  if (started) {
    frame = _latest;
  }
  return started;
}

void AnimationFrameDispatcher::requestFrameLocked() {
  if (_source == nullptr || _frameRequested || !_foreground) {
    return;
  }
  _frameRequested = true;
  _source->requestFrame();
}

void AnimationFrameDispatcher::onFrame(int64_t frameTimeNanos) {
  std::vector<Delivery> pending;
  AnimationFrame frame;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _frameRequested = false;
    if (!_foreground) {
      // Asked for before the app left the foreground: what waits for a frame
      // gets the first one after it is back (see setForeground).
      return;
    }
    const int64_t elapsed = frameTimeNanos - _latest.timeNanos;
    if (_latest.id > 0 && elapsed >= kShortestInterval &&
        elapsed <= kLongestInterval) {
      _intervalNanos = elapsed;
    }
    _latest = {_latest.id + 1, frameTimeNanos};
    frame = _latest;
    pending.swap(_pending);
    if (!pending.empty() || _waiters > 0) {
      _idleFrames = 0;
    } else {
      _idleFrames++;
    }
    if (_idleFrames < kIdleFramesBeforeStop) {
      requestFrameLocked();
    }
  }
  _frameStarted.notify_all();
  for (auto &delivery : pending) {
    if (auto frames = delivery.frames.lock()) {
      frames->deliver(frame, delivery.request);
    }
  }
}

// RuntimeAnimationFrames

RuntimeAnimationFrames::RuntimeAnimationFrames(jsi::Runtime &runtime, Kind kind)
    : _runtime(runtime), _kind(kind) {}

std::shared_ptr<RuntimeAnimationFrames>
RuntimeAnimationFrames::forRuntime(jsi::Runtime &runtime) {
  auto existing = std::static_pointer_cast<RuntimeAnimationFrames>(
      runtime.getRuntimeData(runtimeDataUUID()));
  if (existing) {
    return existing;
  }
  using RuntimeKind = async::RuntimeContext::RuntimeKind;
  Kind kind = Kind::Delegated;
  if (AnimationFrameDispatcher::shared().hasSource()) {
    switch (async::RuntimeContext::runtimeKindOf(runtime)) {
    case RuntimeKind::Main:
      if (async::RuntimeContext::mainInvoker() != nullptr) {
        kind = Kind::Main;
      }
      break;
    case RuntimeKind::UI:
      break;
    case RuntimeKind::Other:
      kind = Kind::Waiting;
      break;
    }
  }
  auto frames = std::make_shared<RuntimeAnimationFrames>(runtime, kind);
  runtime.setRuntimeData(runtimeDataUUID(), frames);
  return frames;
}

double RuntimeAnimationFrames::request(jsi::Runtime &runtime,
                                       jsi::Function callback) {
  const double handle = _nextHandle++;
  _callbacks.emplace(handle,
                     std::make_shared<jsi::Function>(std::move(callback)));
  if (!_scheduled) {
    schedule(runtime);
  }
  return handle;
}

void RuntimeAnimationFrames::cancel(double handle) {
  _callbacks.erase(handle);
  if (_flushing != nullptr) {
    _flushing->erase(handle);
  }
}

void RuntimeAnimationFrames::schedule(jsi::Runtime &runtime) {
  switch (_kind) {
  case Kind::Main: {
    _scheduled = true;
    const uint64_t request = ++_requests;
    _request.store(request, std::memory_order_release);
    auto &dispatcher = AnimationFrameDispatcher::shared();
    AnimationFrameDispatcher::AnimationFrame frame;
    // The frame we are in has not run here: a loop that overran its frame
    // runs in this one, unless its last run was in its frame too.
    if (_lastRun.load(std::memory_order_acquire) != Run::kInFrame &&
        dispatcher.currentFrameAfter(
            _lastFrameId.load(std::memory_order_acquire), frame)) {
      _inFrameRequest.store(request, std::memory_order_release);
      post(frame, request, Run::kInFrame);
    }
    // And on the next animation frame at the latest: whichever comes first
    // runs the callbacks, and the other finds the request answered.
    dispatcher.requestDelivery(shared_from_this(), request);
    return;
  }
  case Kind::Waiting:
    scheduleWaiting(runtime, 0);
    _scheduled = true;
    return;
  case Kind::Delegated:
    scheduleDelegated(runtime);
    _scheduled = true;
    return;
  }
}

void RuntimeAnimationFrames::deliver(
    const AnimationFrameDispatcher::AnimationFrame &frame, uint64_t request) {
  if (_request.load(std::memory_order_acquire) != request) {
    // Answered since.
    return;
  }
  if (!_running.load(std::memory_order_acquire)) {
    post(frame, request, Run::kOnFrame);
    return;
  }
  // The callbacks of an earlier frame are still running: a loop that asks for
  // its next frame first, and overruns. Its next run is in this frame, when
  // the one going on came on an animation frame, and on a frame that starts
  // once it is over otherwise.
  if (_lastRun.load(std::memory_order_acquire) == Run::kOnFrame &&
      _inFrameRequest.exchange(request, std::memory_order_acq_rel) != request) {
    post(frame, request, Run::kInFrame);
  }
  AnimationFrameDispatcher::shared().requestDelivery(shared_from_this(),
                                                     request);
}

void RuntimeAnimationFrames::post(
    const AnimationFrameDispatcher::AnimationFrame &frame, uint64_t request,
    Run run) {
  auto invoker = async::RuntimeContext::mainInvoker();
  if (invoker == nullptr) {
    return;
  }
  std::weak_ptr<RuntimeAnimationFrames> weak = weak_from_this();
  facebook::react::CallFunc task = [weak, frame, request,
                                    run](jsi::Runtime &runtime) {
    auto self = weak.lock();
    // A runtime created since (a reload) has frames of its own.
    if (self == nullptr || &runtime != &self->_runtime) {
      return;
    }
    // Answered already, by the other run posted for it (see schedule).
    if (self->_request.load(std::memory_order_acquire) != request) {
      return;
    }
    self->_lastRun.store(run, std::memory_order_release);
    // One run per frame of the display, counted from when the run starts:
    // posted for a frame and started in a later one (the runtime was busy),
    // this is the run of the frame it starts in.
    const auto latest = AnimationFrameDispatcher::shared().latestFrame();
    const auto &current = latest.id > frame.id ? latest : frame;
    self->flush(runtime, current.id, timestampOf(runtime, current.timeNanos));
  };
  if (run == Run::kInFrame) {
    // The scheduler runs its tasks by expiry, and an idle task expires after
    // everything that waits: React's updates and the app's other tasks go
    // first.
    invoker->invokeAsync(facebook::react::SchedulerPriority::IdlePriority,
                         std::move(task));
  } else {
    // Without a priority, which is the scheduler's highest: a frame of the
    // display goes ahead of what waits, like React Native's own timers.
    invoker->invokeAsync(std::move(task));
  }
}

void RuntimeAnimationFrames::scheduleDelegated(jsi::Runtime &runtime) {
  auto requestAnimationFrame =
      getGlobalFunction(runtime, "requestAnimationFrame");
  std::weak_ptr<RuntimeAnimationFrames> weak = weak_from_this();
  auto onFrame = jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forAscii(runtime, "RNWGPUAnimationFrame"), 1,
      [weak](jsi::Runtime &runtime, const jsi::Value &, const jsi::Value *args,
             size_t count) -> jsi::Value {
        auto self = weak.lock();
        if (self == nullptr || &runtime != &self->_runtime) {
          return jsi::Value::undefined();
        }
        double timestamp = 0;
        if (count > 0 && args[0].isNumber()) {
          timestamp = args[0].asNumber();
        } else {
          timestamp =
              timestampOf(runtime, AnimationFrameDispatcher::nowNanos());
        }
        self->flush(runtime, 0, timestamp);
        return jsi::Value::undefined();
      });
  requestAnimationFrame.call(runtime, std::move(onFrame));
}

void RuntimeAnimationFrames::scheduleWaiting(jsi::Runtime &runtime,
                                             double delayMs) {
  auto setTimeout = getGlobalFunction(runtime, "setTimeout");
  std::weak_ptr<RuntimeAnimationFrames> weak = weak_from_this();
  auto tick = jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forAscii(runtime, "RNWGPUAnimationFrameTick"),
      0,
      [weak](jsi::Runtime &runtime, const jsi::Value &, const jsi::Value *,
             size_t) -> jsi::Value {
        auto self = weak.lock();
        if (self == nullptr || &runtime != &self->_runtime) {
          return jsi::Value::undefined();
        }
        AnimationFrameDispatcher::AnimationFrame frame;
        if (!AnimationFrameDispatcher::shared().waitForFrameAfter(
                self->_lastFrameId.load(std::memory_order_acquire),
                kWaitTimeoutNanos, frame)) {
          // No frames (the app is in the background): give the runtime
          // back, and ask again later.
          self->scheduleWaiting(runtime, kRetryDelayMs);
          return jsi::Value::undefined();
        }
        self->flush(runtime, frame.id, timestampOf(runtime, frame.timeNanos));
        return jsi::Value::undefined();
      });
  setTimeout.call(runtime, std::move(tick), jsi::Value(delayMs));
}

double RuntimeAnimationFrames::timestampOf(jsi::Runtime &runtime,
                                           int64_t frameTimeNanos) {
  // The frame's time on the runtime's performance.now() clock, whatever its
  // origin: now, minus how long ago the frame started.
  const double ageMs =
      static_cast<double>(AnimationFrameDispatcher::nowNanos() -
                          frameTimeNanos) /
      1e6;
  auto performance = runtime.global().getProperty(runtime, "performance");
  if (performance.isObject()) {
    auto object = performance.asObject(runtime);
    auto now = object.getProperty(runtime, "now");
    if (now.isObject() && now.asObject(runtime).isFunction(runtime)) {
      auto value = now.asObject(runtime).asFunction(runtime).callWithThis(
          runtime, object);
      if (value.isNumber()) {
        return value.asNumber() - (ageMs > 0 ? ageMs : 0);
      }
    }
  }
  return static_cast<double>(frameTimeNanos) / 1e6;
}

void RuntimeAnimationFrames::flush(jsi::Runtime &runtime, uint64_t frameId,
                                   double timestampMs) {
  _scheduled = false;
  _request.store(0, std::memory_order_release);
  if (frameId > 0) {
    _lastFrameId.store(frameId, std::memory_order_release);
  }
  _running.store(true, std::memory_order_release);
  // The callbacks requested before this frame; one requested while they run
  // goes to the next frame, and one cancelled meanwhile does not run.
  std::map<double, std::shared_ptr<jsi::Function>> callbacks;
  callbacks.swap(_callbacks);
  _flushing = &callbacks;
  std::exception_ptr first = nullptr;
  while (!callbacks.empty()) {
    auto callback = callbacks.begin()->second;
    callbacks.erase(callbacks.begin());
    try {
      callback->call(runtime, jsi::Value(timestampMs));
    } catch (...) {
      // Like a browser, the other callbacks of the frame still run.
      if (first == nullptr) {
        first = std::current_exception();
      }
    }
  }
  _flushing = nullptr;
  _running.store(false, std::memory_order_release);
  if (first != nullptr) {
    std::rethrow_exception(first);
  }
}

// AnimationFramesHostObject

jsi::Value AnimationFramesHostObject::get(jsi::Runtime &runtime,
                                          const jsi::PropNameID &name) {
  auto property = name.utf8(runtime);
  if (property == kPropRequest) {
    return jsi::Function::createFromHostFunction(
        runtime, name, 1,
        [](jsi::Runtime &runtime, const jsi::Value &, const jsi::Value *args,
           size_t count) -> jsi::Value {
          if (count < 1 || !args[0].isObject() ||
              !args[0].asObject(runtime).isFunction(runtime)) {
            throw jsi::JSError(runtime, "requestAnimationFrame: the first "
                                        "argument must be a function.");
          }
          auto callback = args[0].asObject(runtime).asFunction(runtime);
          return jsi::Value(
              RuntimeAnimationFrames::forRuntime(runtime)->request(
                  runtime, std::move(callback)));
        });
  }
  if (property == kPropCancel) {
    return jsi::Function::createFromHostFunction(
        runtime, name, 1,
        [](jsi::Runtime &runtime, const jsi::Value &, const jsi::Value *args,
           size_t count) -> jsi::Value {
          if (count >= 1 && args[0].isNumber()) {
            RuntimeAnimationFrames::forRuntime(runtime)->cancel(
                args[0].asNumber());
          }
          return jsi::Value::undefined();
        });
  }
  return jsi::Value::undefined();
}

std::vector<jsi::PropNameID>
AnimationFramesHostObject::getPropertyNames(jsi::Runtime &runtime) {
  std::vector<jsi::PropNameID> names;
  names.push_back(jsi::PropNameID::forAscii(runtime, kPropRequest));
  names.push_back(jsi::PropNameID::forAscii(runtime, kPropCancel));
  return names;
}

} // namespace rnwgpu
