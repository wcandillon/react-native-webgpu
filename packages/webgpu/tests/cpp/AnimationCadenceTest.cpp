#include <gtest/gtest.h>

#include <cstdint>

#include "AnimationCadence.h"

namespace {

using rnwgpu::AnimationCadence;

constexpr int64_t kMillis = 1'000'000;
// An arbitrary start: the steady clock has no fixed origin.
constexpr int64_t kStart = 7'000 * kMillis;
constexpr int64_t kIdleAfter = 500 * kMillis;

TEST(AnimationCadenceTest, TheFirstPresentIsNotAnAnimationFrame) {
  AnimationCadence cadence;
  EXPECT_FALSE(cadence.onFramePresented(kStart));
}

TEST(AnimationCadenceTest, AnAnimationIsReportedOnceAtItsFirstFrame) {
  AnimationCadence cadence;
  cadence.onFramePresented(kStart);

  EXPECT_TRUE(cadence.onFramePresented(kStart + 16 * kMillis));
  EXPECT_FALSE(cadence.onFramePresented(kStart + 33 * kMillis));
  EXPECT_FALSE(cadence.onFramePresented(kStart + 50 * kMillis));
}

TEST(AnimationCadenceTest, AnimatingNeedsPresentsAtMostTwentyFiveMillisecondsApart) {
  AnimationCadence atTheThreshold;
  atTheThreshold.onFramePresented(kStart);
  AnimationCadence pastTheThreshold;
  pastTheThreshold.onFramePresented(kStart);

  EXPECT_TRUE(atTheThreshold.onFramePresented(kStart + AnimationCadence::kAnimatingIntervalNanos));
  EXPECT_FALSE(pastTheThreshold.onFramePresented(kStart + AnimationCadence::kAnimatingIntervalNanos + 1));
}

TEST(AnimationCadenceTest, AFewRedrawsASecondNeverAnimate) {
  AnimationCadence cadence;
  for (int64_t present = 0; present < 10; present++) {
    EXPECT_FALSE(cadence.onFramePresented(kStart + present * 200 * kMillis));
  }
  EXPECT_EQ(cadence.releaseIfIdle(kStart + 1'800 * kMillis, kIdleAfter), 0);
}

TEST(AnimationCadenceTest, StopsAnimatingTheIdleTimeAfterTheLastAnimationFrame) {
  AnimationCadence cadence;
  cadence.onFramePresented(kStart);
  cadence.onFramePresented(kStart + 16 * kMillis);

  EXPECT_EQ(cadence.releaseIfIdle(kStart + 100 * kMillis, kIdleAfter), kIdleAfter - 84 * kMillis);
  EXPECT_EQ(cadence.releaseIfIdle(kStart + 16 * kMillis + kIdleAfter, kIdleAfter), 0);
}

TEST(AnimationCadenceTest, SlowRedrawsDoNotKeepItAnimating) {
  AnimationCadence cadence;
  cadence.onFramePresented(kStart);
  cadence.onFramePresented(kStart + 16 * kMillis);

  cadence.onFramePresented(kStart + 300 * kMillis);
  cadence.onFramePresented(kStart + 600 * kMillis);

  EXPECT_EQ(cadence.releaseIfIdle(kStart + 600 * kMillis, kIdleAfter), 0);
}

TEST(AnimationCadenceTest, AnAnimationStillRunningIsNotReleased) {
  AnimationCadence cadence;
  cadence.onFramePresented(kStart);
  EXPECT_TRUE(cadence.onFramePresented(kStart + 16 * kMillis));

  EXPECT_GT(cadence.releaseIfIdle(kStart + 20 * kMillis, kIdleAfter), 0);

  EXPECT_FALSE(cadence.onFramePresented(kStart + 32 * kMillis));
}

TEST(AnimationCadenceTest, TheNextAnimationIsReportedOnceTheLastWasReleased) {
  AnimationCadence cadence;
  cadence.onFramePresented(kStart);
  EXPECT_TRUE(cadence.onFramePresented(kStart + 16 * kMillis));
  EXPECT_EQ(cadence.releaseIfIdle(kStart + 16 * kMillis + kIdleAfter, kIdleAfter), 0);

  cadence.onFramePresented(kStart + 2'000 * kMillis);
  EXPECT_TRUE(cadence.onFramePresented(kStart + 2'016 * kMillis));
}

} // namespace
