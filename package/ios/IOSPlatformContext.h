#pragma once

#include "PlatformContext.h"

namespace rnwgpu {

class IOSPlatformContext : public PlatformContext {
public:
  IOSPlatformContext() = default;
  ~IOSPlatformContext() = default;

  wgpu::Surface makeSurface(void* surface, int width, int height) override;
};

} // namespace rnwgpu