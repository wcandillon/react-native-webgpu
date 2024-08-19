#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct ImageData {
  void *data;
  size_t size;
  size_t width;
  size_t height;
  wgpu::TextureFormat format;
};

class PlatformContext {
public:
  PlatformContext() = default;
  virtual ~PlatformContext() = default;

  virtual wgpu::Surface makeSurface(wgpu::Instance instance, void *surface,
                                    int width, int height) = 0;
  virtual ImageData createImageBitmap(std::string blobId, double offset,
                                      double size) = 0;
};

} // namespace rnwgpu
