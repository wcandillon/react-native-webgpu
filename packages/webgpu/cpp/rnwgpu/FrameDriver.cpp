#include "FrameDriver.h"

#include <memory>
#include <utility>
#include <vector>

namespace jsi = facebook::jsi;

namespace rnwgpu {

FrameDriver &FrameDriver::getInstance() {
  static FrameDriver instance;
  return instance;
}

void FrameDriver::setPlatformVSync(std::function<void()> start,
                                   std::function<void()> stop) {
  std::lock_guard<std::mutex> lock(_mutex);
  _start = std::move(start);
  _stop = std::move(stop);
}

void FrameDriver::requestPresent(
    int contextId, std::shared_ptr<SurfaceInfo> surface,
    std::shared_ptr<async::RuntimeScheduler> scheduler) {
  if (!surface || !scheduler) {
    return;
  }

  std::function<void()> startToCall;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _pending[contextId] = {std::move(surface), std::move(scheduler)};
    _idleFrames = 0;
    if (!_running && _start) {
      _running = true;
      startToCall = _start;
    }
  }

  // Invoked outside the lock: the platform start hops to the UI thread.
  if (startToCall) {
    startToCall();
  }
}

void FrameDriver::cancelPresent(int contextId) {
  std::lock_guard<std::mutex> lock(_mutex);
  _pending.erase(contextId);
}

void FrameDriver::onVSync() {
  std::vector<Pending> toPresent;
  std::function<void()> stopToCall;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_pending.empty()) {
      toPresent.reserve(_pending.size());
      for (auto &entry : _pending) {
        toPresent.push_back(std::move(entry.second));
      }
      _pending.clear();
      _idleFrames = 0;
    } else if (_running && ++_idleFrames >= kMaxIdleFrames) {
      _running = false;
      stopToCall = _stop;
    }
  }

  for (auto &pending : toPresent) {
    auto surface = pending.surface;
    pending.scheduler->scheduleOnJS(
        [surface](jsi::Runtime & /*runtime*/) { surface->presentFrame(); });
  }

  if (stopToCall) {
    stopToCall();
  }
}

} // namespace rnwgpu
