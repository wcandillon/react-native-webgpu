#pragma once

#include "PlatformContext.h"

namespace rnwgpu {

class ApplePlatformContext : public PlatformContext {
public:
  ApplePlatformContext() = default;
  ~ApplePlatformContext() = default;

  wgpu::Surface makeSurface(wgpu::Instance instance, void *surface, int width,
                            int height) override;

  ImageData createImageBitmap(std::string blobId, double offset,
                              double size) override;
};

} // namespace rnwgpu
