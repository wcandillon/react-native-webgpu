#pragma once

#include <atomic>
#include <cstdint>

namespace rnwgpu {

// Decides from the times a canvas presents whether it animates, for its
// refresh rate vote: a frame presented right after its predecessor is an
// animation frame, and the first of an animation is the one to report. Once no
// animation frame came for the idle time the caller asks about, the animation
// has stopped and the next one is reported again. Lock-free, since it runs on
// every present.
class AnimationCadence {
public:
  // Presents this close together are an animation (faster than 40 fps).
  static constexpr int64_t kAnimatingIntervalNanos = 25'000'000;

  // A frame presented at nowNanos. True when it starts an animation that has
  // not been reported yet.
  bool onFramePresented(int64_t nowNanos) {
    const int64_t previous = _lastPresent.exchange(nowNanos);
    if (nowNanos - previous > kAnimatingIntervalNanos) {
      return false;
    }
    _lastAnimating.store(nowNanos);
    return !_reported.exchange(true);
  }

  // How much longer after nowNanos the animation must stay stopped before it
  // counts as idle, or 0 once it has: the next animation frame is then
  // reported again.
  int64_t releaseIfIdle(int64_t nowNanos, int64_t idleAfterNanos) {
    const int64_t idle = nowNanos - _lastAnimating.load();
    if (idle < idleAfterNanos) {
      return idleAfterNanos - idle;
    }
    _reported.store(false);
    return 0;
  }

private:
  // INT64_MIN / 2 rather than a sentinel: the first present is never "right
  // after" it, and the subtractions above don't overflow.
  std::atomic<int64_t> _lastPresent{INT64_MIN / 2};
  std::atomic<int64_t> _lastAnimating{INT64_MIN / 2};
  // The current animation has been reported.
  std::atomic<bool> _reported{false};
};

} // namespace rnwgpu
