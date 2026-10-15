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
  // JS-facing brand. installConstructor() exposes this as
  // globalThis[CLASS_NAME] and keys the NativeObjectRegistry / worklet
  // serializer off it, so it must NOT be "VideoFrame": that would install our
  // constructor over the WebCodecs `VideoFrame` global and shadow it at
  // runtime. "NativeVideoFrame" matches the public TypeScript type (see
  // src/types.ts) and keeps the two namespaces distinct. The C++ class stays
  // `VideoFrame` for brevity; only the brand is namespaced.
  static constexpr const char *CLASS_NAME = "NativeVideoFrame";

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
    // acquireFenceFd: the sync fd the producer of the frame signals once it
    // has written the buffer (Android), as a BigInt, or null when the frame
    // has none (Apple platforms, a released frame). Borrowed: the frame
    // closes it on release(), and importSharedFence() duplicates it.
    // Installed by hand: the typed getters have no null.
    {
      auto getter = jsi::Function::createFromHostFunction(
          runtime, jsi::PropNameID::forUtf8(runtime, "get_acquireFenceFd"), 0,
          [](jsi::Runtime &rt, const jsi::Value &thisVal,
             const jsi::Value * /*args*/, size_t /*count*/) -> jsi::Value {
            auto native = VideoFrame::fromValue(rt, thisVal);
            const auto &handle = native->_handle;
            if (handle.handle == nullptr || handle.acquireFenceFd < 0) {
              return jsi::Value::null();
            }
            return jsi::BigInt::fromUint64(
                rt, static_cast<uint64_t>(handle.acquireFenceFd));
          });
      auto objectCtor = runtime.global().getPropertyAsObject(runtime, "Object");
      auto defineProperty =
          objectCtor.getPropertyAsFunction(runtime, "defineProperty");
      jsi::Object descriptor(runtime);
      descriptor.setProperty(runtime, "get", getter);
      descriptor.setProperty(runtime, "enumerable", true);
      descriptor.setProperty(runtime, "configurable", true);
      defineProperty.call(
          runtime, prototype,
          jsi::String::createFromUtf8(runtime, "acquireFenceFd"), descriptor);
    }
  }

private:
  VideoFrameHandle _handle;
};

} // namespace rnwgpu
