#include "IdleFlusher.h"

#include <algorithm>
#include <thread>
#include <utility>

namespace rnwgpu {

namespace {

thread_local bool tInFlush = false;

// The flush locks this thread holds, innermost first. cancel() is allowed
// from inside a flush, and from what releasing the flush function sets off, so
// it has to tell that a lock it would try is one this thread already owns.
struct Holding {
  const void *state;
  Holding *previous;
};
thread_local Holding *tHolding = nullptr;

class ScopedHolding {
public:
  explicit ScopedHolding(const void *state) : _node{state, tHolding} {
    tHolding = &_node;
  }
  ~ScopedHolding() { tHolding = _node.previous; }

  ScopedHolding(const ScopedHolding &) = delete;
  ScopedHolding &operator=(const ScopedHolding &) = delete;

private:
  Holding _node;
};

bool holding(const void *state) {
  for (const Holding *node = tHolding; node != nullptr; node = node->previous) {
    if (node->state == state) {
      return true;
    }
  }
  return false;
}

// Runs `flush` on this thread, marked as a flush. Exceptions stop here: a
// flush has no one to report to.
void runMarked(const std::function<void()> &flush) {
  const bool outer = tInFlush;
  tInFlush = true;
  try {
    flush();
  } catch (...) {
  }
  tInFlush = outer;
}

} // namespace

IdleFlusher::IdleFlusher(std::function<void()> flush,
                         std::chrono::milliseconds quiet,
                         std::chrono::milliseconds maxDelay, bool background)
    : _state(std::make_shared<State>(std::move(flush), quiet, maxDelay,
                                     background)) {}

IdleFlusher::~IdleFlusher() { cancel(); }

bool IdleFlusher::flushing() { return tInFlush; }

IdleFlusher::Clock::time_point IdleFlusher::State::due() const {
  return std::min(lastTouch + quiet, firstTouch + maxDelay);
}

void IdleFlusher::touch() {
  auto &state = *_state;
  std::lock_guard<std::mutex> lock(state.mutex);
  if (state.cancelled) {
    return;
  }
  const auto now = Clock::now();
  if (!state.dirty) {
    state.dirty = true;
    state.firstTouch = now;
  }
  state.lastTouch = now;
  if (state.background) {
    if (!state.threadStarted) {
      state.threadStarted = true;
      // The thread keeps the state alive, so it outlives this object if it has
      // to.
      std::thread([shared = _state] { shared->run(); }).detach();
    }
    state.wake.notify_one();
  }
}

void IdleFlusher::flushNow() {
  auto &state = *_state;
  std::lock_guard<std::mutex> flushLock(state.flushMutex);
  ScopedHolding scoped(&state);
  bool due = false;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.dirty && !state.cancelled) {
      state.dirty = false;
      due = true;
    }
  }
  if (due && state.flush) {
    runMarked(state.flush);
  }
  state.releaseIfCancelled();
}

void IdleFlusher::cancel() {
  auto &state = *_state;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.cancelled = true;
  }
  state.wake.notify_all();
  if (holding(&state)) {
    // Called from inside a flush, or from what releasing its function set off:
    // whoever holds the lock drops the function when it is done.
    return;
  }
  // Release what the flush holds now if that is possible; a flush in progress
  // does it when it ends.
  std::unique_lock<std::mutex> flushLock(state.flushMutex, std::try_to_lock);
  if (flushLock.owns_lock()) {
    ScopedHolding scoped(&state);
    state.releaseIfCancelled();
  }
}

void IdleFlusher::finish() {
  auto &state = *_state;
  std::lock_guard<std::mutex> flushLock(state.flushMutex);
  ScopedHolding scoped(&state);
  bool pending;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    pending = state.dirty && !state.cancelled;
    state.dirty = false;
    state.cancelled = true;
  }
  state.wake.notify_all();
  if (pending && state.flush) {
    runMarked(state.flush);
  }
  state.releaseIfCancelled();
}

void IdleFlusher::stop() {
  cancel();
  auto &state = *_state;
  std::lock_guard<std::mutex> flushLock(state.flushMutex);
  ScopedHolding scoped(&state);
  state.releaseIfCancelled();
}

void IdleFlusher::State::run() {
  std::unique_lock<std::mutex> lock(mutex);
  while (!cancelled) {
    if (!dirty) {
      wake.wait(lock);
      continue;
    }
    if (Clock::now() < due()) {
      wake.wait_until(lock, due());
      continue;
    }
    lock.unlock();
    runFlush();
    lock.lock();
  }
}

// Decides again under the flush lock whether the flush is still due, so a
// finish() or flushNow() that ran in between has already taken it, and one
// that is still to come waits for this one.
void IdleFlusher::State::runFlush() {
  std::lock_guard<std::mutex> flushLock(flushMutex);
  ScopedHolding scoped(this);
  bool shouldRun = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!cancelled && dirty && Clock::now() >= due()) {
      dirty = false;
      shouldRun = true;
    }
  }
  if (shouldRun && flush) {
    runMarked(flush);
  }
  releaseIfCancelled();
}

// What the function captured is released outside `mutex`: letting go of a
// device can call back into code that cancels.
void IdleFlusher::State::releaseIfCancelled() {
  bool isCancelled;
  {
    std::lock_guard<std::mutex> lock(mutex);
    isCancelled = cancelled;
  }
  if (isCancelled) {
    std::function<void()> released = std::move(flush);
    flush = nullptr;
  }
}

} // namespace rnwgpu
