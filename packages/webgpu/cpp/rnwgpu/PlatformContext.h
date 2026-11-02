#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
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
  // Whether the RGB channels are premultiplied by alpha. Each platform decoder
  // records the representation it produced; createImageBitmap then converts to
  // the representation requested through premultiplyAlpha, and
  // copyExternalImageToTexture converts again to match the destination's
  // premultipliedAlpha. Defaults to true because most native decoders
  // (CoreGraphics, Android Bitmap) hand back premultiplied pixels.
  bool premultiplied = true;
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

// The color space of a YUV frame, as its producer reports it: the YCbCr matrix
// and range that encode the samples, and the primaries and transfer function of
// the RGB they encode. Each member is optional: what the producer does not know
// (a camera buffer carries no color metadata, a container may record only some
// of it) is resolved at import time (GPUExternalTexture::Create) from the
// buffer and the platform's norm. Dawn converts the samples to RGB in the
// destination color space while sampling the external texture.
struct VideoFrameColorSpace {
  std::optional<wgpu::ColorSpacePrimariesDawn> primaries;
  std::optional<wgpu::ColorSpaceTransferDawn> transfer;
  std::optional<wgpu::ColorSpaceYCbCrRangeDawn> range;
  std::optional<wgpu::ColorSpaceYCbCrMatrixDawn> matrix;
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
  // The color space of the samples. Only meaningful when pixelFormat == NV12:
  // from the CVPixelBuffer attachments on Apple platforms, from the Image's
  // data space (API 33) or the container's color info on Android.
  VideoFrameColorSpace colorSpace;
  // Android: the acquire fence of the frame as a sync file descriptor, or -1.
  // The producer (video decoder, camera) may still be writing the buffer when
  // the consumer gets it; the GPU work that reads the frame waits on this fence
  // (imported as a shared-fence-sync-fd into BeginAccess). Owned by the handle:
  // the deleter closes it. Importing it into Dawn duplicates it, so the frame
  // can be imported more than once.
  int acquireFenceFd = -1;
  std::function<void()> deleter;
};

// The samples of the NV12 test frame of createTestVideoFrame: pure red and
// pure green in BT.709 limited range (Y 16..235, Cb/Cr 16..240), so the frame
// converts back to exactly those colors in sRGB. Shared by both platforms so
// the e2e spec sees the same frame on each.
namespace test_yuv_frame {
constexpr uint8_t kRedY = 63;
constexpr uint8_t kRedCb = 102;
constexpr uint8_t kRedCr = 240;
constexpr uint8_t kGreenY = 173;
constexpr uint8_t kGreenCb = 42;
constexpr uint8_t kGreenCr = 26;
} // namespace test_yuv_frame

// Platform-implemented video source that hands out fresh IOSurface /
// AHardwareBuffer-backed frames as a video plays, with the playback controls
// of an HTMLMediaElement (times in seconds).
class IVideoPlayer {
public:
  virtual ~IVideoPlayer() = default;

  // Returns the latest decoded frame, or an empty handle (handle == nullptr)
  // when no new frame is ready yet. Each non-empty return retains its backing
  // surface; the VideoFrame wrapper releases it on destruction.
  virtual VideoFrameHandle copyLatestFrame() = 0;

  virtual void play() = 0;
  virtual void pause() = 0;
  virtual bool paused() = 0;

  // Playback position and length, in seconds. The duration is 0 until the
  // metadata of the media has loaded.
  virtual double currentTime() = 0;
  virtual void seek(double seconds) = 0;
  virtual double duration() = 0;

  virtual double volume() = 0;
  virtual void setVolume(double volume) = 0;
  virtual bool loop() = 0;
  virtual void setLoop(bool loop) = 0;

  // The geometry of the frames, 0 until the metadata has loaded: their coded
  // size, the clockwise rotation (0, 90, 180 or 270) to apply when displaying
  // them, and the nominal frame rate of the video track.
  virtual uint32_t videoWidth() = 0;
  virtual uint32_t videoHeight() = 0;
  virtual int rotation() = 0;
  virtual double frameRate() = 0;
};

// A request to rasterize a native React Native view (looked up by its React
// tag) for GPUQueue.drawElementImageToTexture.
//
// The source rectangle is expressed in the view's own coordinate space, in
// density-independent points (the units of React Native layout). A
// non-positive sourceWidth/sourceHeight means "the whole view". The
// destination size is in pixels: the platform rasterizes the source rectangle
// scaled to exactly width x height.
struct ViewSnapshotRequest {
  int viewTag = 0;
  double sourceX = 0;
  double sourceY = 0;
  double sourceWidth = 0;
  double sourceHeight = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  // Whether the caller can draw a GPU frame (a VideoFrameHandle) into the
  // destination itself. A platform that renders the view into a GPU buffer
  // then hands that buffer over instead of reading the pixels back.
  bool allowFrame = false;
  // iOS: whether to flush pending layout and layer changes before drawing
  // (drawViewHierarchyInRect's afterScreenUpdates), so a snapshot taken right
  // after a state change reflects it; false draws the last committed frame
  // without that round trip. Android renders the views' current display
  // lists either way.
  bool afterScreenUpdates = true;
};

class PlatformContext {
public:
  PlatformContext() = default;
  virtual ~PlatformContext() = default;

  // The platform context of the currently installed RNWebGPUManager, for API
  // objects (e.g. GPUQueue) that are created without a reference to it. Set on
  // install, cleared on teardown; a dev reload replaces it.
  static void setCurrent(std::shared_ptr<PlatformContext> context) {
    std::lock_guard<std::mutex> lock(currentMutex());
    currentSlot() = std::move(context);
  }
  static std::shared_ptr<PlatformContext> current() {
    std::lock_guard<std::mutex> lock(currentMutex());
    return currentSlot();
  }

  // Rasterize a native view (see ViewSnapshotRequest). The implementation hops
  // to the platform UI thread, so this is asynchronous: exactly one of
  // onPixels / onFrame / onError is invoked, from an arbitrary thread
  // (typically the UI thread). Callers must not touch JSI in the callbacks.
  //
  // onPixels receives the view as CPU pixels: tightly packed (no row
  // padding), 4 bytes per pixel, `format` rgba8unorm or bgra8unorm depending
  // on what the platform rasterizer produces natively, and `premultiplied`
  // reporting its alpha representation.
  //
  // onFrame, only when the request allows it, receives the GPU buffer the
  // platform rendered the view into (Android 13 and above: the
  // HardwareRenderer's AHardwareBuffer, with the fence that signals when the
  // drawing is done), as an RGBA frame of premultiplied pixels. The receiver
  // owns the frame and releases it through its deleter.
  virtual void snapshotView(const ViewSnapshotRequest &request,
                            std::function<void(ImageData)> onPixels,
                            std::function<void(VideoFrameHandle)> onFrame,
                            std::function<void(std::string)> onError) = 0;

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
  // SharedTextureMemory example. BGRA8 is an RGB gradient; NV12 is a biplanar
  // YUV frame, BT.709 limited range, red on the left half and green on the
  // right, which exercises the color conversion of importExternalTexture.
  // Throws a message containing "unsupported" where the platform cannot
  // allocate or fill the YUV layout (Android before API 29).
  virtual VideoFrameHandle createTestVideoFrame(uint32_t width, uint32_t height,
                                                VideoPixelFormat format) = 0;

  // Open a video file at `path` for playback. The returned player yields
  // IOSurface / AHardwareBuffer-backed frames via copyLatestFrame().
  //
  // `format` selects the requested pixel layout. BGRA8 is the easiest target
  // for a regular sampled GPUTexture; NV12 is the right shape for the
  // importExternalTexture path (zero-copy biplanar YUV).
  virtual std::unique_ptr<IVideoPlayer>
  createVideoPlayer(const std::string &path, VideoPixelFormat format) = 0;

  // Wrap a CVPixelBufferRef (Apple) or AHardwareBuffer* (Android) pointer
  // obtained from another library (typically VisionCamera's
  // Frame.getNativeBuffer().pointer) as one of our VideoFrame handles.
  //
  // We CFRetain / AHardwareBuffer_acquire on the way in, so callers can
  // safely release their own reference immediately after.
  virtual VideoFrameHandle wrapNativeBuffer(void *pointer) = 0;

  // Retain a native window obtained from another library, to present to it
  // (an ANativeWindow* on Android, a CAMetalLayer* on Apple platforms), and
  // return the releaser that balances that reference once the surface is done
  // with it. The releaser can run on any thread.
  virtual std::function<void(void *)> retainNativeWindow(void *window) = 0;

  // Write a small procedurally-generated test video to a temporary location
  // and return its absolute path. Lets the SharedTextureMemory example play
  // a real decoded video without bundling an asset.
  virtual std::string writeTestVideoFile() = 0;

private:
  static std::mutex &currentMutex() {
    static std::mutex mutex;
    return mutex;
  }
  static std::shared_ptr<PlatformContext> &currentSlot() {
    static std::shared_ptr<PlatformContext> slot;
    return slot;
  }
};

} // namespace rnwgpu
