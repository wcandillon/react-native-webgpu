#pragma once

#include "PlatformContext.h"
#include <functional>
#include <string>

namespace rnwgpu {

class ApplePlatformContext : public PlatformContext {
public:
  // Resolves a React tag to its native view (an unretained
  // UIView* / NSView* bridged to void*, or nullptr). Called on the main thread
  // only. Provided by WebGPUModule from React Native's view registry.
  using ViewLookup = std::function<void *(int)>;

  explicit ApplePlatformContext(ViewLookup viewLookup = nullptr);
  ~ApplePlatformContext() = default;

  void snapshotView(const ViewSnapshotRequest &request,
                    std::function<void(ImageData)> onSuccess,
                    std::function<void(std::string)> onError) override;

  wgpu::Surface makeSurface(wgpu::Instance instance, void *surface, int width,
                            int height) override;

  ImageData createImageBitmap(std::string blobId, double offset,
                              double size) override;

  void
  createImageBitmapAsync(std::string blobId, double offset, double size,
                         std::function<void(ImageData)> onSuccess,
                         std::function<void(std::string)> onError) override;

  ImageData createImageBitmapFromData(std::span<const uint8_t> data) override;

  void createImageBitmapFromDataAsync(
      std::span<const uint8_t> data, std::function<void(ImageData)> onSuccess,
      std::function<void(std::string)> onError) override;

  VideoFrameHandle loadVideoFrame(const std::string &path) override;

  VideoFrameHandle createTestVideoFrame(uint32_t width,
                                        uint32_t height) override;

  std::unique_ptr<IVideoPlayer>
  createVideoPlayer(const std::string &path,
                    VideoPixelFormat format) override;

  std::string writeTestVideoFile() override;

  VideoFrameHandle wrapNativeBuffer(void *pointer) override;

private:
  ViewLookup _viewLookup;
};

} // namespace rnwgpu
