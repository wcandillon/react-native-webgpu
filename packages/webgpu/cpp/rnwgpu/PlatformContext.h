#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct ImageData {
  std::vector<uint8_t> data;
  size_t width;
  size_t height;
  wgpu::TextureFormat format;
};

class PlatformContext {
public:
  PlatformContext() = default;
  virtual ~PlatformContext() = default;

  virtual ImageData createImageBitmap(std::string blobId, double offset,
                                      double size) = 0;

  // Async version that performs image decoding on a background thread
  virtual void createImageBitmapAsync(
      std::string blobId, double offset, double size,
      std::function<void(ImageData)> onSuccess,
      std::function<void(std::string)> onError) = 0;

  // Create ImageBitmap from raw encoded image bytes (PNG/JPEG/etc.)
  virtual ImageData createImageBitmapFromData(std::span<const uint8_t> data) = 0;

  virtual void createImageBitmapFromDataAsync(
      std::span<const uint8_t> data, std::function<void(ImageData)> onSuccess,
      std::function<void(std::string)> onError) = 0;
};

} // namespace rnwgpu
