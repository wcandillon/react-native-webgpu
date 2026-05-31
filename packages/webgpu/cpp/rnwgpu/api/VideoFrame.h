#pragma once

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "JSIConverter.h"
#include "NativeObject.h"

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

  VideoFrame(void *handle, uint32_t width, uint32_t height,
             std::function<void()> deleter)
      : NativeObject(CLASS_NAME), _handle(handle), _width(width),
        _height(height), _deleter(std::move(deleter)) {}

  ~VideoFrame() override { release(); }

  std::string getBrand() { return CLASS_NAME; }

  // The native handle (IOSurfaceRef / AHardwareBuffer*) as a uintptr_t value.
  // Exposed as a BigInt on the JS side.
  void *getHandle() { return _handle; }
  uint32_t getWidth() { return _width; }
  uint32_t getHeight() { return _height; }

  void release() {
    if (_deleter) {
      _deleter();
      _deleter = nullptr;
    }
    _handle = nullptr;
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand", &VideoFrame::getBrand);
    installGetter(runtime, prototype, "handle", &VideoFrame::getHandle);
    installGetter(runtime, prototype, "width", &VideoFrame::getWidth);
    installGetter(runtime, prototype, "height", &VideoFrame::getHeight);
    installMethod(runtime, prototype, "release", &VideoFrame::release);
  }

private:
  void *_handle;
  uint32_t _width;
  uint32_t _height;
  std::function<void()> _deleter;
};

} // namespace rnwgpu
