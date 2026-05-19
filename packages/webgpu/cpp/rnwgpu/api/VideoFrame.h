#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "JSIConverter.h"
#include "NativeObject.h"
#include "PlatformContext.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

// VideoFrame is a small RAII wrapper around a platform-specific native handle
// (IOSurfaceRef-backed CVPixelBuffer on Apple, AHardwareBuffer on Android).
// It surfaces the raw handle as a BigInt to JS so callers can hand it to
// GPUDevice.importSharedTextureMemory, and owns a deleter so the underlying
// object stays alive until the JS object is GC'd (or release() is called).
class VideoFrame : public NativeObject<VideoFrame> {
public:
  static constexpr const char *CLASS_NAME = "VideoFrame";

  explicit VideoFrame(VideoFrameHandle handle)
      : NativeObject(CLASS_NAME), _handle(std::move(handle)) {}

  ~VideoFrame() override { release(); }

  std::string getBrand() { return CLASS_NAME; }

  // The native handle (IOSurfaceRef / AHardwareBuffer*) as a uintptr_t value.
  // Exposed as a BigInt on the JS side.
  void *getHandle() { return _handle.handle; }
  uint32_t getWidth() { return _handle.width; }
  uint32_t getHeight() { return _handle.height; }

  // Pixel format as a JS-visible string: "bgra8" | "nv12".
  std::string getPixelFormat() {
    return _handle.pixelFormat == VideoPixelFormat::NV12 ? "nv12" : "bgra8";
  }

  // Direct access to the underlying handle, for use by importExternalTexture /
  // importSharedTextureMemory inside the C++ layer (not exposed to JS).
  const VideoFrameHandle &handle() const { return _handle; }

  void release() {
    if (_handle.deleter) {
      _handle.deleter();
      _handle.deleter = nullptr;
    }
    _handle.handle = nullptr;
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &VideoFrame::getBrand);
    installGetter(runtime, prototype, "handle", &VideoFrame::getHandle);
    installGetter(runtime, prototype, "width", &VideoFrame::getWidth);
    installGetter(runtime, prototype, "height", &VideoFrame::getHeight);
    installGetter(runtime, prototype, "pixelFormat",
                  &VideoFrame::getPixelFormat);
    installMethod(runtime, prototype, "release", &VideoFrame::release);
  }

private:
  VideoFrameHandle _handle;
};

} // namespace rnwgpu
