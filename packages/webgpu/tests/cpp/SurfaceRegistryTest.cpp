#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "FakeWebGPU.h"
#include "SurfaceRegistry.h"

namespace {

using rnwgpu::SurfaceInfo;
using rnwgpu::SurfaceRegistry;

constexpr int kWidth = 100;
constexpr int kHeight = 50;

struct NativeWindow {
  int releases = 0;
};

// The registry is a process-wide singleton: every test starts and ends with it
// empty, with no WebGPU call Dawn would reject and no object leaked. The window
// outlives the entries, which the registry keeps until TearDown.
class SurfaceRegistryTest : public ::testing::Test {
protected:
  void SetUp() override {
    registry().clear();
    fake_webgpu::reset();
    _liveObjects = fake_webgpu::liveObjects();
  }

  void TearDown() override {
    registry().clear();
    EXPECT_EQ(fake_webgpu::errors(), std::vector<std::string>{});
    EXPECT_EQ(fake_webgpu::liveObjects(), _liveObjects);
  }

  static SurfaceRegistry &registry() { return SurfaceRegistry::getInstance(); }

  std::shared_ptr<SurfaceInfo> attachWindow(int id) {
    return registry().attachSurface(id, nullptr, kWidth, kHeight, &_window,
                                    fake_webgpu::createSurface(),
                                    [this](void *) { _window.releases++; });
  }

  NativeWindow _window;

private:
  int _liveObjects = 0;
};

TEST_F(SurfaceRegistryTest, FindsTheEntryItCreated) {
  auto created = registry().getSurfaceInfoOrCreate(1, nullptr, kWidth, kHeight);

  EXPECT_EQ(registry().getSurfaceInfo(1), created);
  EXPECT_EQ(registry().getSurfaceInfoOrCreate(1, nullptr, kWidth, kHeight),
            created);
  EXPECT_EQ(registry().getSurfaceInfo(2), nullptr);
}

// React's StrictMode runs the Canvas cleanup right after mount and keeps the
// context the first effect run made.
TEST_F(SurfaceRegistryTest, HandsARetiredEntryBackToTheContextThatHoldsIt) {
  auto held = registry().getSurfaceInfoOrCreate(1, nullptr, kWidth, kHeight);

  registry().removeSurfaceInfoIfDetached(1);

  EXPECT_EQ(registry().getSurfaceInfo(1), nullptr);
  EXPECT_EQ(registry().getSurfaceInfoOrCreate(1, nullptr, kWidth, kHeight),
            held);
}

TEST_F(SurfaceRegistryTest, ForgetsARetiredEntryNothingHolds) {
  std::weak_ptr<SurfaceInfo> retired =
      registry().getSurfaceInfoOrCreate(1, nullptr, kWidth, kHeight);

  registry().removeSurfaceInfoIfDetached(1);

  EXPECT_TRUE(retired.expired());
  EXPECT_NE(registry().getSurfaceInfoOrCreate(1, nullptr, kWidth, kHeight),
            nullptr);
}

TEST_F(SurfaceRegistryTest, KeepsAnEntryWhoseViewOwnsASurface) {
  attachWindow(1);

  registry().removeSurfaceInfoIfDetached(1);
  EXPECT_NE(registry().getSurfaceInfo(1), nullptr);
  EXPECT_EQ(_window.releases, 0);

  registry().removeSurfaceInfo(1);
  EXPECT_EQ(registry().getSurfaceInfo(1), nullptr);
  EXPECT_EQ(_window.releases, 1);
}

TEST_F(SurfaceRegistryTest, AttachesToTheEntryAContextAlreadyHolds) {
  auto context = registry().getSurfaceInfoOrCreate(1, nullptr, kWidth, kHeight);

  auto attached = attachWindow(1);

  EXPECT_EQ(attached, context);
  EXPECT_TRUE(context->hasNativeSurface());
  EXPECT_EQ(context->getNativeInfo().nativeSurface, &_window);
}

TEST_F(SurfaceRegistryTest, ClearForgetsEveryEntry) {
  registry().getSurfaceInfoOrCreate(1, nullptr, kWidth, kHeight);
  registry().getSurfaceInfoOrCreate(2, nullptr, kWidth, kHeight);

  registry().clear();

  EXPECT_EQ(registry().getSurfaceInfo(1), nullptr);
  EXPECT_EQ(registry().getSurfaceInfo(2), nullptr);
}

} // namespace
