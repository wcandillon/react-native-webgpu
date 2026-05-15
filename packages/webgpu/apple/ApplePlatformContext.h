#pragma once

#include "PlatformContext.h"
#include <string>

namespace rnwgpu {

class ApplePlatformContext : public PlatformContext {
public:
  ApplePlatformContext();
  ~ApplePlatformContext() = default;

  wgpu::Surface makeSurface(wgpu::Instance instance, void *surface, int width,
                            int height) override;

  void configureSurfaceColor(void *nativeSurface,
                             const SurfaceColorConfig &config) override;

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
};

} // namespace rnwgpu
