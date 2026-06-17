#pragma once

#include <memory>
#include <string>

#include "NativeObject.h"

#include "ElementCaptureRegistry.h"

#include "ArrayBuffer.h"
#include "Canvas.h"
#include "GPU.h"
#include "GPUCanvasContext.h"
#include "ImageBitmap.h"
#include "PlatformContext.h"
#include "VideoFrame.h"
#include "VideoPlayer.h"

#include <ReactCommon/CallInvoker.h>

#include "JSIConverter.h"
#include "Promise.h"

#if defined(__ANDROID__)
#include <android/hardware_buffer.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace rnwgpu {

namespace jsi = facebook::jsi;

struct Blob {
  std::string blobId;
  double size;
  double offset;
  std::string type;
  std::string name;
};

// JSIConverter specialization must be declared before use
template <> struct JSIConverter<std::shared_ptr<Blob>> {
  static std::shared_ptr<Blob>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    if (!outOfBounds && arg.isObject()) {
      auto result = std::make_unique<Blob>();
      auto val = arg.asObject(runtime);
      if (val.hasProperty(runtime, "_data")) {
        auto value = val.getPropertyAsObject(runtime, "_data");
        result->blobId = JSIConverter<std::string>::fromJSI(
            runtime, value.getProperty(runtime, "blobId"), false);
        result->size = JSIConverter<double>::fromJSI(
            runtime, value.getProperty(runtime, "size"), false);
        result->offset = JSIConverter<double>::fromJSI(
            runtime, value.getProperty(runtime, "offset"), false);
      }
      return result;
    } else {
      throw std::runtime_error("Invalid Blob::fromJSI()");
    }
  }
  static jsi::Value toJSI(jsi::Runtime &runtime, std::shared_ptr<Blob> arg) {
    throw std::runtime_error("Invalid Blob::toJSI()");
  }
};

class RNWebGPU : public NativeObject<RNWebGPU> {
public:
  static constexpr const char *CLASS_NAME = "RNWebGPU";

  explicit RNWebGPU(std::shared_ptr<GPU> gpu,
                    std::shared_ptr<PlatformContext> platformContext,
                    std::shared_ptr<facebook::react::CallInvoker> callInvoker)
      : NativeObject(CLASS_NAME), _gpu(gpu), _platformContext(platformContext),
        _callInvoker(callInvoker) {}

  std::shared_ptr<GPU> getGPU() { return _gpu; }

  bool getFabric() { return true; }

  std::shared_ptr<GPUCanvasContext>
  MakeWebGPUCanvasContext(int contextId, float width, float height) {
    auto ctx =
        std::make_shared<GPUCanvasContext>(_gpu, contextId, width, height);
    return ctx;
  }

  jsi::Value createImageBitmap(jsi::Runtime &runtime,
                               const jsi::Value & /*thisVal*/,
                               const jsi::Value *args, size_t count) {
    if (count < 1) {
      throw jsi::JSError(runtime,
                         "createImageBitmap requires a Blob or ArrayBuffer "
                         "argument");
    }

    auto platformContext = _platformContext;
    auto callInvoker = _callInvoker;

    // Check if the argument is an ArrayBuffer or ArrayBufferView
    // (TypedArray / DataView). Only a real buffer source is run through the
    // ArrayBuffer converter, which validates byteOffset/byteLength against the
    // backing buffer and throws on an out-of-bounds (or spoofed) view. Anything
    // else (e.g. a Blob) is left to the fall-through path below, so its errors
    // are not misreported as bounds errors here.
    if (args[0].isObject()) {
      auto obj = args[0].getObject(runtime);

      bool isBufferSource = obj.isArrayBuffer(runtime);
      if (!isBufferSource && obj.hasProperty(runtime, "buffer")) {
        auto bufferProp = obj.getProperty(runtime, "buffer");
        isBufferSource = bufferProp.isObject() &&
                         bufferProp.getObject(runtime).isArrayBuffer(runtime);
      }

      if (isBufferSource) {
        // Bounds violations propagate out of fromJSI so the call rejects
        // rather than reading out of bounds below.
        auto buffer = JSIConverter<std::shared_ptr<ArrayBuffer>>::fromJSI(
            runtime, args[0], false);
        std::span<const uint8_t> data{buffer->data(), buffer->size()};

        // Copy bytes on the JS thread: the ArrayBuffer pointer is into
        // JS-owned memory that can be GC'd
        std::vector<uint8_t> dataCopy(data.begin(), data.end());

        return Promise::createPromise(
            runtime,
            [platformContext, callInvoker, dataCopy = std::move(dataCopy)](
                jsi::Runtime & /*runtime*/,
                std::shared_ptr<Promise> promise) mutable {
              platformContext->createImageBitmapFromDataAsync(
                  dataCopy,
                  [callInvoker, promise](ImageData imageData) {
                    auto imageBitmap = std::make_shared<ImageBitmap>(imageData);
                    callInvoker->invokeAsync([promise, imageBitmap]() {
                      promise->resolve(
                          JSIConverter<std::shared_ptr<ImageBitmap>>::toJSI(
                              promise->runtime, imageBitmap));
                    });
                  },
                  [callInvoker, promise](std::string error) {
                    callInvoker->invokeAsync(
                        [promise, error]() { promise->reject(error); });
                  });
            });
      }
    }

    // Fall through to existing Blob path
    auto blob =
        JSIConverter<std::shared_ptr<Blob>>::fromJSI(runtime, args[0], false);
    std::string blobId = blob->blobId;
    double offset = blob->offset;
    double size = blob->size;

    return Promise::createPromise(
        runtime,
        [platformContext, callInvoker, blobId, offset,
         size](jsi::Runtime & /*runtime*/, std::shared_ptr<Promise> promise) {
          platformContext->createImageBitmapAsync(
              blobId, offset, size,
              [callInvoker, promise](ImageData imageData) {
                auto imageBitmap = std::make_shared<ImageBitmap>(imageData);
                callInvoker->invokeAsync([promise, imageBitmap]() {
                  promise->resolve(
                      JSIConverter<std::shared_ptr<ImageBitmap>>::toJSI(
                          promise->runtime, imageBitmap));
                });
              },
              [callInvoker, promise](std::string error) {
                callInvoker->invokeAsync(
                    [promise, error]() { promise->reject(error); });
              });
        });
  }

  std::shared_ptr<VideoFrame> loadVideoFrame(std::string path) {
    auto frame = _platformContext->loadVideoFrame(path);
    return std::make_shared<VideoFrame>(std::move(frame));
  }

  std::shared_ptr<VideoFrame> createTestVideoFrame(double width, double height) {
    auto frame = _platformContext->createTestVideoFrame(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    return std::make_shared<VideoFrame>(std::move(frame));
  }

  // Wrap a CVPixelBufferRef / AHardwareBuffer* pointer (typed as void* via
  // BigInt on the JS side) into one of our VideoFrames. The native side
  // CFRetains / acquires so the caller can release immediately.
  std::shared_ptr<VideoFrame> createVideoFrameFromNativeBuffer(void *pointer) {
    auto handle = _platformContext->wrapNativeBuffer(pointer);
    return std::make_shared<VideoFrame>(std::move(handle));
  }

  std::shared_ptr<VideoPlayer>
  createVideoPlayer(std::string path, std::optional<std::string> pixelFormat) {
    auto format = (pixelFormat && pixelFormat.value() == "nv12")
                      ? VideoPixelFormat::NV12
                      : VideoPixelFormat::BGRA8;
    auto impl = _platformContext->createVideoPlayer(path, format);
    return std::make_shared<VideoPlayer>(std::move(impl));
  }

  std::string writeTestVideoFile() {
    return _platformContext->writeTestVideoFile();
  }

  // "HTML in Canvas": retrieve (and remove) the off-screen capture produced by
  // NativeWebGPUModule.captureElement(), keyed by the token that call resolved
  // with. Returns { handle, width, height, fence, fenceType } where handle is
  // the AHardwareBuffer*/IOSurface as a BigInt. The caller takes ownership of
  // handle and must call releaseCapturedElement(handle) once the import has
  // taken its own reference.
  jsi::Value consumeCapturedElement(jsi::Runtime &runtime,
                                    const jsi::Value & /*thisVal*/,
                                    const jsi::Value *args, size_t count) {
    if (count < 1 || !args[0].isNumber()) {
      throw jsi::JSError(runtime,
                         "consumeCapturedElement requires a numeric token");
    }
    int token = static_cast<int>(args[0].asNumber());
    auto entry = ElementCaptureRegistry::getInstance().consume(token);
    if (!entry.has_value()) {
      throw jsi::JSError(runtime,
                         "consumeCapturedElement: no capture for token " +
                             std::to_string(token));
    }
    jsi::Object result(runtime);
    result.setProperty(
        runtime, "handle",
        jsi::BigInt::fromUint64(runtime,
                                reinterpret_cast<uint64_t>(entry->handle)));
    result.setProperty(runtime, "width",
                       jsi::Value(static_cast<double>(entry->width)));
    result.setProperty(runtime, "height",
                       jsi::Value(static_cast<double>(entry->height)));
    // v1 waits for producer completion on the CPU, so there is no wait fence:
    // report 0 and let the JS orchestration skip importSharedFence.
    uint64_t fence =
        entry->fenceFd < 0 ? 0u : static_cast<uint64_t>(entry->fenceFd);
    result.setProperty(runtime, "fence",
                       jsi::BigInt::fromUint64(runtime, fence));
    result.setProperty(runtime, "fenceType",
                       jsi::String::createFromUtf8(runtime, "sync-fd"));
    return result;
  }

  // Release the native handle returned by consumeCapturedElement, after the
  // SharedTextureMemory import has taken its own reference.
  void releaseCapturedElement(void *handle) {
    if (handle == nullptr) {
      return;
    }
#if defined(__ANDROID__)
    AHardwareBuffer_release(static_cast<AHardwareBuffer *>(handle));
#elif defined(__APPLE__)
    CFRelease(static_cast<CFTypeRef>(handle));
#endif
  }

  std::shared_ptr<Canvas> getNativeSurface(int contextId) {
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto info = registry.getSurfaceInfo(contextId);
    if (info == nullptr) {
      return std::make_shared<Canvas>(nullptr, 0, 0);
    }
    auto nativeInfo = info->getNativeInfo();
    return std::make_shared<Canvas>(nativeInfo.nativeSurface, nativeInfo.width,
                                    nativeInfo.height);
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "fabric", &RNWebGPU::getFabric);
    installGetter(runtime, prototype, "gpu", &RNWebGPU::getGPU);
    installMethod(runtime, prototype, "createImageBitmap",
                  &RNWebGPU::createImageBitmap);
    installMethod(runtime, prototype, "getNativeSurface",
                  &RNWebGPU::getNativeSurface);
    installMethod(runtime, prototype, "MakeWebGPUCanvasContext",
                  &RNWebGPU::MakeWebGPUCanvasContext);
    installMethod(runtime, prototype, "loadVideoFrame",
                  &RNWebGPU::loadVideoFrame);
    installMethod(runtime, prototype, "createTestVideoFrame",
                  &RNWebGPU::createTestVideoFrame);
    installMethod(runtime, prototype, "createVideoFrameFromNativeBuffer",
                  &RNWebGPU::createVideoFrameFromNativeBuffer);
    installMethod(runtime, prototype, "createVideoPlayer",
                  &RNWebGPU::createVideoPlayer);
    installMethod(runtime, prototype, "writeTestVideoFile",
                  &RNWebGPU::writeTestVideoFile);
    installMethod(runtime, prototype, "consumeCapturedElement",
                  &RNWebGPU::consumeCapturedElement);
    installMethod(runtime, prototype, "releaseCapturedElement",
                  &RNWebGPU::releaseCapturedElement);
  }

private:
  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
  std::shared_ptr<facebook::react::CallInvoker> _callInvoker;
};

} // namespace rnwgpu
