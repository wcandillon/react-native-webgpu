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

// A native handle to a video frame that can be imported into a
// GPUSharedTextureMemory.
//
//   - handle is an IOSurfaceRef on Apple platforms
//   - handle is an AHardwareBuffer* on Android
//
// The deleter is responsible for releasing the underlying backing object
// (CVPixelBuffer / AHardwareBuffer) and must be called exactly once. The
// VideoFrame JS wrapper handles this on destruction.
struct VideoFrameHandle {
  void *handle = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  std::function<void()> deleter;
};

class PlatformContext {
public:
  PlatformContext() = default;
  virtual ~PlatformContext() = default;

  virtual wgpu::Surface makeSurface(wgpu::Instance instance, void *surface,
                                    int width, int height) = 0;
  virtual ImageData createImageBitmap(std::string blobId, double offset,
                                      double size) = 0;

  // Async version that performs image decoding on a background thread
  virtual void
  createImageBitmapAsync(std::string blobId, double offset, double size,
                         std::function<void(ImageData)> onSuccess,
                         std::function<void(std::string)> onError) = 0;

  // Create ImageBitmap from raw encoded image bytes (PNG/JPEG/etc.)
  virtual ImageData
  createImageBitmapFromData(std::span<const uint8_t> data) = 0;

  virtual void
  createImageBitmapFromDataAsync(std::span<const uint8_t> data,
                                 std::function<void(ImageData)> onSuccess,
                                 std::function<void(std::string)> onError) = 0;

  // Decode the first video frame of `path` (a local file path) into a
  // native, GPU-shareable surface. Used by the SharedTextureMemory example;
  // not intended as a long-term media-loading API.
  virtual VideoFrameHandle loadVideoFrame(const std::string &path) = 0;

  // Create a synthetic, GPU-shareable IOSurface/AHardwareBuffer filled with a
  // generated test pattern. Avoids the need to bundle a video asset for the
  // SharedTextureMemory example.
  virtual VideoFrameHandle createTestVideoFrame(uint32_t width,
                                                uint32_t height) = 0;
};

} // namespace rnwgpu
