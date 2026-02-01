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

  ImageData createImageBitmap(std::string blobId, double offset,
                              double size) override;

  void createImageBitmapAsync(
      std::string blobId, double offset, double size,
      std::function<void(ImageData)> onSuccess,
      std::function<void(std::string)> onError) override;
};

} // namespace rnwgpu
