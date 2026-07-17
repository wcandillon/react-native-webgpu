#pragma once

#include <memory>
#include <string>

#include "NativeObject.h"

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

  std::shared_ptr<VideoFrame> createTestVideoFrame(double width,
                                                   double height) {
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

  // Retires a canvas context from the JS side (Canvas unmount cleanup).
  // Registry entries have two owners, split by whether a native surface is
  // attached:
  // - A native view currently owns a surface (or one is pending): its own
  //   teardown (MetalView dealloc / WebGPUViewManager.onDropViewInstance)
  //   removes the entry. Skipping here keeps React StrictMode safe: its
  //   simulated unmount re-runs JS effects without unmounting native views,
  //   and removing the entry then would orphan the still-attached surface.
  // - No native surface: the JS side is the last owner and removes the entry.
  void destroyContext(int contextId) {
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    if (auto info = registry.getSurfaceInfo(contextId)) {
      if (info->hasNativeSurface()) {
        return;
      }
    }
    registry.removeSurfaceInfo(contextId);
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "fabric", &RNWebGPU::getFabric);
    installGetter(runtime, prototype, "gpu", &RNWebGPU::getGPU);
    installMethod(runtime, prototype, "createImageBitmap",
                  &RNWebGPU::createImageBitmap);
    installMethod(runtime, prototype, "getNativeSurface",
                  &RNWebGPU::getNativeSurface);
    installMethod(runtime, prototype, "destroyContext",
                  &RNWebGPU::destroyContext);
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
  }

private:
  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
  std::shared_ptr<facebook::react::CallInvoker> _callInvoker;
};

} // namespace rnwgpu
