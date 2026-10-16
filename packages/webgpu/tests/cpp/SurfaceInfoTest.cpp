#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "FakeWebGPU.h"
#include "SurfaceRegistry.h"

namespace {

using rnwgpu::SurfaceInfo;

constexpr int kWidth = 100;
constexpr int kHeight = 50;

// A platform window: SurfaceInfo hands it back through its releaser.
struct NativeWindow {
  int releases = 0;
};

rnwgpu::NativeSurfaceReleaser releaserFor(NativeWindow *window) {
  return [window](void *) { window->releases++; };
}

void configure(SurfaceInfo *info, const wgpu::Device &device) {
  wgpu::SurfaceConfiguration configuration;
  configuration.device = device;
  configuration.format = wgpu::TextureFormat::RGBA8Unorm;
  configuration.usage = wgpu::TextureUsage::RenderAttachment;
  info->configure(configuration, {});
}

void renderFrame(SurfaceInfo *info) {
  info->getCurrentTexture();
  info->presentFrame();
}

// Every test ends with no WebGPU call Dawn would reject and no object leaked.
class SurfaceInfoTest : public ::testing::Test {
protected:
  void SetUp() override {
    fake_webgpu::reset();
    _liveObjects = fake_webgpu::liveObjects();
  }

  void TearDown() override {
    EXPECT_EQ(fake_webgpu::errors(), std::vector<std::string>{});
    EXPECT_EQ(fake_webgpu::liveObjects(), _liveObjects);
  }

private:
  int _liveObjects = 0;
};

TEST_F(SurfaceInfoTest, RendersOffscreenUntilASurfaceAttaches) {
  auto device = fake_webgpu::createDevice();
  SurfaceInfo info(nullptr, kWidth, kHeight);
  configure(&info, device);

  auto texture = info.getCurrentTexture();

  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(fake_webgpu::surfaceOf(texture), 0);
  EXPECT_EQ(texture.GetWidth(), static_cast<uint32_t>(kWidth));
  EXPECT_EQ(texture.GetHeight(), static_cast<uint32_t>(kHeight));
  info.presentFrame();
  EXPECT_TRUE(fake_webgpu::submittedCopies().empty());
}

TEST_F(SurfaceInfoTest, RefusesAFrameBeforeConfigure) {
  SurfaceInfo info(nullptr, kWidth, kHeight);

  EXPECT_THROW(info.getCurrentTexture(), std::runtime_error);
}

TEST_F(SurfaceInfoTest, PresentsEveryFrameOnTheSurfaceItWasAcquiredFrom) {
  auto device = fake_webgpu::createDevice();
  auto surface = fake_webgpu::createSurface();
  NativeWindow window;
  SurfaceInfo info(nullptr, kWidth, kHeight);
  info.attachSurface(&window, surface, releaserFor(&window));
  configure(&info, device);

  for (int frame = 1; frame <= 3; frame++) {
    auto texture = info.getCurrentTexture();
    EXPECT_EQ(fake_webgpu::surfaceOf(texture), fake_webgpu::surfaceId(surface));
    info.presentFrame();
    EXPECT_EQ(fake_webgpu::presentCount(surface), frame);
  }
  EXPECT_EQ(fake_webgpu::configureCount(surface), 1);
}

TEST_F(SurfaceInfoTest, AdoptsAnAttachedSurfaceOnlyAtAFrameBoundary) {
  auto device = fake_webgpu::createDevice();
  auto surface = fake_webgpu::createSurface();
  NativeWindow window;
  SurfaceInfo info(nullptr, kWidth, kHeight);
  configure(&info, device);
  info.getCurrentTexture();

  info.attachSurface(&window, surface, releaserFor(&window));
  // What RNWebGPUManager::flushPendingSurfaceTransition does from the JS
  // thread while another thread renders.
  info.applyPendingAttach();

  EXPECT_EQ(fake_webgpu::configureCount(surface), 0);
  info.presentFrame();
  EXPECT_EQ(fake_webgpu::configureCount(surface), 1);
}

TEST_F(SurfaceInfoTest, ShowsTheFrameRenderedBeforeTheSurfaceOnIt) {
  auto device = fake_webgpu::createDevice();
  auto surface = fake_webgpu::createSurface();
  NativeWindow window;
  SurfaceInfo info(nullptr, kWidth, kHeight);
  configure(&info, device);
  auto offscreen = info.getCurrentTexture();
  info.attachSurface(&window, surface, releaserFor(&window));

  info.presentFrame();

  auto copies = fake_webgpu::submittedCopies();
  ASSERT_EQ(copies.size(), 1u);
  EXPECT_EQ(copies[0].sourceTexture, fake_webgpu::textureId(offscreen));
  EXPECT_EQ(copies[0].destinationSurface, fake_webgpu::surfaceId(surface));
  EXPECT_EQ(copies[0].width, static_cast<uint32_t>(kWidth));
  EXPECT_EQ(copies[0].height, static_cast<uint32_t>(kHeight));
  EXPECT_EQ(fake_webgpu::presentCount(surface), 1);
  // The copy needs CopyDst on the surface, on top of the usage configured.
  auto usage = fake_webgpu::lastConfiguration(surface).usage;
  EXPECT_NE(usage & WGPUTextureUsage_CopyDst, 0u);
  EXPECT_NE(usage & WGPUTextureUsage_RenderAttachment, 0u);
}

TEST_F(SurfaceInfoTest, DropsAFrameWhoseSurfaceIsDestroyedMidFrame) {
  auto device = fake_webgpu::createDevice();
  auto surface = fake_webgpu::createSurface();
  NativeWindow window;
  SurfaceInfo info(nullptr, kWidth, kHeight);
  info.attachSurface(&window, surface, releaserFor(&window));
  configure(&info, device);
  auto texture = info.getCurrentTexture();
  ASSERT_EQ(fake_webgpu::surfaceOf(texture), fake_webgpu::surfaceId(surface));

  info.switchToOffscreen();
  info.presentFrame();

  EXPECT_EQ(fake_webgpu::presentCount(surface), 0);
  EXPECT_EQ(window.releases, 1);
  auto next = info.getCurrentTexture();
  EXPECT_EQ(fake_webgpu::surfaceOf(next), 0);
  info.presentFrame();
}

TEST_F(SurfaceInfoTest, ReleasesAPendingSurfaceThatIsReplacedBeforeAdoption) {
  auto device = fake_webgpu::createDevice();
  auto replaced = fake_webgpu::createSurface();
  auto surface = fake_webgpu::createSurface();
  NativeWindow replacedWindow;
  NativeWindow window;
  SurfaceInfo info(nullptr, kWidth, kHeight);

  info.attachSurface(&replacedWindow, replaced, releaserFor(&replacedWindow));
  info.attachSurface(&window, surface, releaserFor(&window));
  EXPECT_EQ(replacedWindow.releases, 1);
  configure(&info, device);
  renderFrame(&info);

  EXPECT_EQ(fake_webgpu::configureCount(replaced), 0);
  EXPECT_EQ(fake_webgpu::configureCount(surface), 1);
  EXPECT_EQ(fake_webgpu::presentCount(surface), 1);
  EXPECT_EQ(window.releases, 0);
}

TEST_F(SurfaceInfoTest, ReleasesEveryWindowItHoldsWhenDestroyed) {
  auto device = fake_webgpu::createDevice();
  NativeWindow adoptedWindow;
  NativeWindow pendingWindow;
  {
    SurfaceInfo info(nullptr, kWidth, kHeight);
    info.attachSurface(&adoptedWindow, fake_webgpu::createSurface(),
                       releaserFor(&adoptedWindow));
    configure(&info, device);
    info.attachSurface(&pendingWindow, fake_webgpu::createSurface(),
                       releaserFor(&pendingWindow));
  }

  EXPECT_EQ(adoptedWindow.releases, 1);
  EXPECT_EQ(pendingWindow.releases, 1);
}

TEST_F(SurfaceInfoTest, ReconfiguresTheSurfaceOnceWhenItIsOutdated) {
  auto device = fake_webgpu::createDevice();
  auto surface = fake_webgpu::createSurface();
  NativeWindow window;
  SurfaceInfo info(nullptr, kWidth, kHeight);
  info.attachSurface(&window, surface, releaserFor(&window));
  configure(&info, device);
  fake_webgpu::failNextAcquire(surface,
                               wgpu::SurfaceGetCurrentTextureStatus::Outdated);

  auto texture = info.getCurrentTexture();

  EXPECT_EQ(fake_webgpu::surfaceOf(texture), fake_webgpu::surfaceId(surface));
  EXPECT_EQ(fake_webgpu::configureCount(surface), 2);
  info.presentFrame();
  EXPECT_EQ(fake_webgpu::presentCount(surface), 1);
}

TEST_F(SurfaceInfoTest, RendersOffscreenWhileTheSurfaceCannotAcquire) {
  auto device = fake_webgpu::createDevice();
  auto surface = fake_webgpu::createSurface();
  NativeWindow window;
  SurfaceInfo info(nullptr, kWidth, kHeight);
  info.attachSurface(&window, surface, releaserFor(&window));
  configure(&info, device);
  fake_webgpu::failNextAcquire(surface,
                               wgpu::SurfaceGetCurrentTextureStatus::Error);

  auto texture = info.getCurrentTexture();
  EXPECT_EQ(fake_webgpu::surfaceOf(texture), 0);
  info.presentFrame();
  EXPECT_EQ(fake_webgpu::presentCount(surface), 0);

  renderFrame(&info);
  EXPECT_EQ(fake_webgpu::presentCount(surface), 1);
}

TEST_F(SurfaceInfoTest, ResizesTheDrawingBufferOnReconfigure) {
  auto device = fake_webgpu::createDevice();
  auto surface = fake_webgpu::createSurface();
  NativeWindow window;
  SurfaceInfo offscreen(nullptr, kWidth, kHeight);
  SurfaceInfo onscreen(nullptr, kWidth, kHeight);
  configure(&offscreen, device);
  onscreen.attachSurface(&window, surface, releaserFor(&window));
  configure(&onscreen, device);

  offscreen.reconfigure(20, 10);
  onscreen.reconfigure(20, 10);

  auto texture = offscreen.getCurrentTexture();
  EXPECT_EQ(texture.GetWidth(), 20u);
  EXPECT_EQ(texture.GetHeight(), 10u);
  offscreen.presentFrame();
  EXPECT_EQ(fake_webgpu::lastConfiguration(surface).width, 20u);
  EXPECT_EQ(fake_webgpu::lastConfiguration(surface).height, 10u);
}

TEST_F(SurfaceInfoTest, AFrameIsCurrentUntilItEndsOrTheSurfaceChanges) {
  auto device = fake_webgpu::createDevice();
  NativeWindow window;
  SurfaceInfo info(nullptr, kWidth, kHeight);
  configure(&info, device);
  uint64_t frame = 0;

  info.getCurrentTexture(&frame);
  EXPECT_TRUE(info.isCurrentFrame(frame));
  info.presentFrame();
  EXPECT_FALSE(info.isCurrentFrame(frame));

  info.getCurrentTexture(&frame);
  info.reconfigure(20, 10);
  EXPECT_FALSE(info.isCurrentFrame(frame));

  info.getCurrentTexture(&frame);
  info.attachSurface(&window, fake_webgpu::createSurface(),
                     releaserFor(&window));
  EXPECT_FALSE(info.isCurrentFrame(frame));
  info.presentFrame();
}

} // namespace
