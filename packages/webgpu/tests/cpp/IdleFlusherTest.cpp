#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include "IdleFlusher.h"

namespace {

using rnwgpu::IdleFlusher;
using std::chrono::milliseconds;

void sleepFor(int ms) { std::this_thread::sleep_for(milliseconds(ms)); }

// Waits for `condition`, for as long as `ms`.
template <typename Condition> bool waitFor(Condition condition, int ms) {
  const auto deadline = std::chrono::steady_clock::now() + milliseconds(ms);
  while (!condition()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    sleepFor(1);
  }
  return true;
}

TEST(IdleFlusher, FlushesOnceAfterThingsGoQuiet) {
  std::atomic<int> flushes{0};
  IdleFlusher flusher([&] { flushes++; }, milliseconds(30), milliseconds(1000),
                      true);

  flusher.touch();
  flusher.touch();
  flusher.touch();

  ASSERT_TRUE(waitFor([&] { return flushes == 1; }, 2000));
  sleepFor(150);
  EXPECT_EQ(flushes, 1);
  flusher.stop();
}

TEST(IdleFlusher, FlushesAgainForTouchesAfterAFlush) {
  std::atomic<int> flushes{0};
  IdleFlusher flusher([&] { flushes++; }, milliseconds(20), milliseconds(1000),
                      true);

  flusher.touch();
  ASSERT_TRUE(waitFor([&] { return flushes == 1; }, 2000));
  flusher.touch();

  EXPECT_TRUE(waitFor([&] { return flushes == 2; }, 2000));
  flusher.stop();
}

TEST(IdleFlusher, NeverFlushesWithoutATouch) {
  std::atomic<int> flushes{0};
  IdleFlusher flusher([&] { flushes++; }, milliseconds(10), milliseconds(50),
                      true);

  sleepFor(150);
  flusher.flushNow();

  EXPECT_EQ(flushes, 0);
  flusher.stop();
}

// Touches that never stop coming must not hold the flush back for ever.
TEST(IdleFlusher, FlushesWhileTouchesKeepComingOnceTheDelayIsUp) {
  std::atomic<int> flushes{0};
  IdleFlusher flusher([&] { flushes++; }, milliseconds(400), milliseconds(1000),
                      true);

  const auto start = std::chrono::steady_clock::now();
  auto lastTouch = start;
  std::chrono::milliseconds longestGap{0};
  int flushesBeforeTheDelay = -1;
  // Touches until two flushes have happened, with a generous limit for a
  // machine that cannot keep up.
  while (flushes < 2 &&
         std::chrono::steady_clock::now() - start < milliseconds(15000)) {
    const auto now = std::chrono::steady_clock::now();
    longestGap = std::max(
        longestGap, std::chrono::duration_cast<milliseconds>(now - lastTouch));
    lastTouch = now;
    flusher.touch();
    sleepFor(10);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (flushesBeforeTheDelay < 0 && elapsed > milliseconds(500) &&
        elapsed < milliseconds(900)) {
      flushesBeforeTheDelay = flushes;
    }
  }

  // Only a run that kept touching without a pause as long as the quiet time
  // says anything about the delay: a machine that stalled that long may have
  // flushed for being quiet.
  if (longestGap < milliseconds(300) && flushesBeforeTheDelay >= 0) {
    EXPECT_EQ(flushesBeforeTheDelay, 0);
  }
  EXPECT_GE(flushes, 2);
  flusher.stop();
}

TEST(IdleFlusher, DoesNotFlushByItselfWithoutABackground) {
  std::atomic<int> flushes{0};
  IdleFlusher flusher([&] { flushes++; }, milliseconds(10), milliseconds(50),
                      false);

  flusher.touch();
  sleepFor(150);

  EXPECT_EQ(flushes, 0);
}

TEST(IdleFlusher, FlushesNowOnTheCallingThreadOnlyWhatWasTouched) {
  std::thread::id flushedOn;
  int flushes = 0;
  IdleFlusher flusher(
      [&] {
        flushes++;
        flushedOn = std::this_thread::get_id();
      },
      milliseconds(10), milliseconds(50), false);

  flusher.flushNow();
  EXPECT_EQ(flushes, 0);

  flusher.touch();
  flusher.flushNow();
  EXPECT_EQ(flushes, 1);
  EXPECT_EQ(flushedOn, std::this_thread::get_id());

  flusher.flushNow();
  EXPECT_EQ(flushes, 1);
}

TEST(IdleFlusher, FlushesNothingOnceCancelled) {
  std::atomic<int> flushes{0};
  IdleFlusher flusher([&] { flushes++; }, milliseconds(20), milliseconds(100),
                      true);

  flusher.touch();
  flusher.cancel();
  flusher.touch();
  flusher.flushNow();
  sleepFor(150);

  EXPECT_EQ(flushes, 0);
  flusher.stop();
}

// What the flush uses (a device) may go as soon as stop() returns.
TEST(IdleFlusher, StopWaitsForAFlushInProgress) {
  std::promise<void> started;
  std::promise<void> release;
  auto released = release.get_future().share();
  IdleFlusher flusher(
      [&started, released] {
        started.set_value();
        released.wait();
      },
      milliseconds(10), milliseconds(50), true);
  flusher.touch();
  started.get_future().wait();

  std::atomic<bool> stopped{false};
  std::thread stopping([&] {
    flusher.stop();
    stopped = true;
  });
  sleepFor(100);
  EXPECT_FALSE(stopped);

  release.set_value();
  stopping.join();
  EXPECT_TRUE(stopped);
}

TEST(IdleFlusher, StopReleasesWhatTheFlushHolds) {
  auto held = std::make_shared<int>(0);
  IdleFlusher flusher([held] {}, milliseconds(10), milliseconds(50), true);
  EXPECT_EQ(held.use_count(), 2);

  flusher.stop();

  EXPECT_EQ(held.use_count(), 1);
}

// Two runtimes can destroy a device at once.
TEST(IdleFlusher, MayBeStoppedFromSeveralThreadsAtOnce) {
  for (int i = 0; i < 50; i++) {
    std::atomic<int> flushes{0};
    IdleFlusher flusher([&] { flushes++; }, milliseconds(1), milliseconds(5),
                        true);
    flusher.touch();
    std::thread first([&] { flusher.stop(); });
    std::thread second([&] { flusher.finish(); });
    flusher.stop();
    first.join();
    second.join();
  }
}

// The flush a device promises at destroy() must not be lost to the thread that
// was just about to run it.
TEST(IdleFlusher, FinishFlushesWhatWasTouchedExactlyOnce) {
  for (int i = 0; i < 200; i++) {
    std::atomic<int> flushes{0};
    IdleFlusher flusher([&] { flushes++; }, milliseconds(1), milliseconds(5),
                        true);
    flusher.touch();
    std::this_thread::sleep_for(std::chrono::microseconds(i % 40 * 50));

    flusher.finish();

    ASSERT_EQ(flushes, 1) << "round " << i;
    sleepFor(0);
  }
}

TEST(IdleFlusher, FinishFlushesNothingThatWasNotTouched) {
  std::atomic<int> flushes{0};
  IdleFlusher flusher([&] { flushes++; }, milliseconds(1), milliseconds(5),
                      true);

  flusher.finish();
  flusher.touch();
  flusher.finish();

  EXPECT_EQ(flushes, 0);
}

// A device lost callback can drop the last reference to the device while Dawn
// holds a lock the flush waits for, so destroying must never wait for one.
TEST(IdleFlusher, DestroyingDoesNotWaitForAFlushInProgress) {
  std::promise<void> started;
  std::promise<void> release;
  auto released = release.get_future().share();
  std::atomic<bool> flushed{false};
  {
    IdleFlusher flusher(
        [&started, released, &flushed] {
          started.set_value();
          released.wait();
          flushed = true;
        },
        milliseconds(1), milliseconds(5), true);
    flusher.touch();
    started.get_future().wait();
  }

  EXPECT_FALSE(flushed);
  release.set_value();
  EXPECT_TRUE(waitFor([&] { return flushed.load(); }, 2000));
}

TEST(IdleFlusher, KnowsWhenItIsInAFlush) {
  bool insideNow = false;
  std::atomic<bool> insideBackground{false};
  std::atomic<int> backgroundFlushes{0};
  bool background = false;
  IdleFlusher flusher(
      [&] {
        if (background) {
          insideBackground = IdleFlusher::flushing();
          backgroundFlushes++;
        } else {
          insideNow = IdleFlusher::flushing();
        }
      },
      milliseconds(300), milliseconds(1000), true);

  EXPECT_FALSE(IdleFlusher::flushing());
  flusher.touch();
  flusher.flushNow();
  EXPECT_TRUE(insideNow);
  EXPECT_FALSE(IdleFlusher::flushing());

  background = true;
  flusher.touch();
  ASSERT_TRUE(waitFor([&] { return backgroundFlushes == 1; }, 2000));
  EXPECT_TRUE(insideBackground);
  flusher.stop();
}

// A device lost callback can run on the thread that is flushing.
TEST(IdleFlusher, MayBeCancelledFromInsideAFlush) {
  auto held = std::make_shared<int>(0);
  std::atomic<IdleFlusher *> self{nullptr};
  std::atomic<int> flushes{0};
  IdleFlusher flusher(
      [held, &self, &flushes] {
        flushes++;
        self.load()->cancel();
      },
      milliseconds(1), milliseconds(5), true);
  self = &flusher;
  EXPECT_EQ(held.use_count(), 2);

  flusher.touch();
  ASSERT_TRUE(waitFor([&] { return flushes == 1; }, 2000));

  // The flush function goes once the flush that cancelled has ended.
  EXPECT_TRUE(waitFor([&] { return held.use_count() == 1; }, 2000));
  flusher.touch();
  sleepFor(50);
  EXPECT_EQ(flushes, 1);
}

TEST(IdleFlusher, MayBeCancelledFromWhatReleasingTheFlushSetsOff) {
  struct CancelsOnRelease {
    explicit CancelsOnRelease(std::atomic<IdleFlusher *> *flusher)
        : flusher(flusher) {}
    CancelsOnRelease(const CancelsOnRelease &) = default;
    ~CancelsOnRelease() {
      if (IdleFlusher *target = flusher->load()) {
        target->cancel();
      }
    }
    std::atomic<IdleFlusher *> *flusher;
  };
  std::atomic<IdleFlusher *> self{nullptr};
  {
    auto releases = std::make_shared<CancelsOnRelease>(&self);
    IdleFlusher flusher([releases] {}, milliseconds(1), milliseconds(5), false);
    self = &flusher;
    releases.reset();

    flusher.touch();
    flusher.stop();
    self = nullptr;
  }
}

// What the flush holds goes when the flush that was running at cancel() ends,
// not only at stop().
TEST(IdleFlusher, ReleasesTheFlushAfterAFlushNowThatWasCancelled) {
  std::promise<void> started;
  std::promise<void> release;
  auto released = release.get_future().share();
  auto held = std::make_shared<int>(0);
  IdleFlusher flusher(
      [held, &started, released] {
        started.set_value();
        released.wait();
      },
      milliseconds(1), milliseconds(5), false);
  flusher.touch();
  std::thread flushing([&] { flusher.flushNow(); });
  started.get_future().wait();

  flusher.cancel();
  EXPECT_EQ(held.use_count(), 2);
  release.set_value();
  flushing.join();

  EXPECT_EQ(held.use_count(), 1);
}

TEST(IdleFlusher, StopsFromADestructorWithAFlushPending) {
  std::atomic<int> flushes{0};
  {
    IdleFlusher flusher([&] { flushes++; }, milliseconds(500),
                        milliseconds(1000), true);
    flusher.touch();
  }

  sleepFor(50);
  EXPECT_EQ(flushes, 0);
}

} // namespace
