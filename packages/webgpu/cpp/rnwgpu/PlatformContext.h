#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

struct ImageData {
  std::vector<uint8_t> data;
  size_t width;
  size_t height;
  wgpu::TextureFormat format;
};

// Pixel layout of a VideoFrame. Determines whether the underlying surface is
// a single RGBA plane or a biplanar Y / CbCr pair.
enum class VideoPixelFormat {
  // Single-plane 8-bit BGRA (default; what RGBA-style sampling expects).
  BGRA8,
  // Biplanar 4:2:0 8-bit Y + interleaved CbCr (NV12). Used for the
  // importExternalTexture path; needs the YUV→RGB conversion matrix below.
  NV12,
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
  VideoPixelFormat pixelFormat = VideoPixelFormat::BGRA8;
  // 3x4 row-major matrix mapping [Y, U, V, 1] → linear [R, G, B]. Pre-computed
  // at decode time from CVPixelBuffer attachments (kCVImageBufferYCbCrMatrixKey
  // + range), with a BT.709 limited-range default. Only meaningful when
  // pixelFormat == NV12.
  float yuvToRgbMatrix[12] = {};
  std::function<void()> deleter;
};

// Platform-implemented video source that hands out fresh IOSurface /
// AHardwareBuffer-backed frames as a video plays.
class IVideoPlayer {
public:
  virtual ~IVideoPlayer() = default;

  // Returns the latest decoded frame, or an empty handle (handle == nullptr)
  // when no new frame is ready yet. Each non-empty return retains its backing
  // surface; the VideoFrame wrapper releases it on destruction.
  virtual VideoFrameHandle copyLatestFrame() = 0;

  virtual void play() = 0;
  virtual void pause() = 0;
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

  // Open a video file at `path` for playback. The returned player yields
  // IOSurface / AHardwareBuffer-backed frames via copyLatestFrame().
  //
  // `format` selects the requested pixel layout. BGRA8 is the easiest target
  // for a regular sampled GPUTexture; NV12 is the right shape for the
  // importExternalTexture path (zero-copy biplanar YUV).
  virtual std::unique_ptr<IVideoPlayer>
  createVideoPlayer(const std::string &path, VideoPixelFormat format) = 0;

  // Write a small procedurally-generated test video to a temporary location
  // and return its absolute path. Lets the SharedTextureMemory example play
  // a real decoded video without bundling an asset.
  virtual std::string writeTestVideoFile() = 0;
};

} // namespace rnwgpu
