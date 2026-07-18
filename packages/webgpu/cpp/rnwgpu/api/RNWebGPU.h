#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "NativeObject.h"

#include "ArrayBuffer.h"
#include "Canvas.h"
#include "GPU.h"
#include "GPUCanvasContext.h"
#include "ImageBitmap.h"
#include "PlatformContext.h"
#include "RNWebGPUSession.h"
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
                    std::shared_ptr<facebook::react::CallInvoker> callInvoker,
                    std::shared_ptr<RNWebGPUSessionState> sessionState,
                    jsi::Runtime &owningRuntime)
      : NativeObject(CLASS_NAME), _gpu(std::move(gpu)),
        _platformContext(std::move(platformContext)),
        _callInvoker(std::move(callInvoker)),
        _sessionState(std::move(sessionState)), _owningRuntime(&owningRuntime) {
  }

  std::shared_ptr<GPU> getGPU() {
    ensureSessionActive();
    return _gpu;
  }

  bool getFabric() { return true; }

  double getSessionId() {
    return _sessionState ? static_cast<double>(_sessionState->id())
                         : static_cast<double>(kInvalidRNWebGPUSessionId);
  }

  std::shared_ptr<GPUCanvasContext>
  MakeWebGPUCanvasContext(int contextId, float width, float height) {
    if (!_sessionState || !_sessionState->isActive()) {
      throw std::runtime_error("WebGPU runtime session is no longer active");
    }
    auto ctx = std::make_shared<GPUCanvasContext>(_gpu, _sessionState,
                                                  contextId, width, height);
    return ctx;
  }

  jsi::Value createImageBitmap(jsi::Runtime &runtime,
                               const jsi::Value & /*thisVal*/,
                               const jsi::Value *args, size_t count) {
    if (!_sessionState || !_sessionState->isActive()) {
      throw jsi::JSError(runtime, "WebGPU runtime session is no longer active");
    }
    // RNWebGPU itself can be boxed into a worklet runtime, but image decoding
    // completes through React Native's main CallInvoker. Creating the Promise
    // anywhere else would settle JSI values on the wrong runtime thread.
    if (&runtime != _owningRuntime) {
      throw jsi::JSError(
          runtime,
          "createImageBitmap is only supported on the main React Native "
          "runtime");
    }
    if (count < 1) {
      throw jsi::JSError(runtime,
                         "createImageBitmap requires a Blob or ArrayBuffer "
                         "argument");
    }

    auto platformContext = _platformContext;
    auto callInvoker = _callInvoker;
    auto sessionState = _sessionState;

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
            runtime, [platformContext, callInvoker, sessionState,
                      dataCopy = std::move(dataCopy)](
                         jsi::Runtime & /*runtime*/,
                         std::shared_ptr<Promise> promise) mutable {
              platformContext->createImageBitmapFromDataAsync(
                  dataCopy,
                  [callInvoker, promise, sessionState](ImageData imageData) {
                    RNWebGPU::settleImageBitmapSuccess(callInvoker, promise,
                                                       sessionState,
                                                       std::move(imageData));
                  },
                  [callInvoker, promise, sessionState](std::string error) {
                    RNWebGPU::settleImageBitmapError(
                        callInvoker, promise, sessionState, std::move(error));
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
        [platformContext, callInvoker, sessionState, blobId, offset,
         size](jsi::Runtime & /*runtime*/, std::shared_ptr<Promise> promise) {
          platformContext->createImageBitmapAsync(
              blobId, offset, size,
              [callInvoker, promise, sessionState](ImageData imageData) {
                RNWebGPU::settleImageBitmapSuccess(
                    callInvoker, promise, sessionState, std::move(imageData));
              },
              [callInvoker, promise, sessionState](std::string error) {
                RNWebGPU::settleImageBitmapError(
                    callInvoker, promise, sessionState, std::move(error));
              });
        });
  }

  std::shared_ptr<VideoFrame> loadVideoFrame(std::string path) {
    ensureSessionActive();
    auto frame = _platformContext->loadVideoFrame(path);
    return std::make_shared<VideoFrame>(std::move(frame));
  }

  std::shared_ptr<VideoFrame> createTestVideoFrame(double width,
                                                   double height) {
    ensureSessionActive();
    auto frame = _platformContext->createTestVideoFrame(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    return std::make_shared<VideoFrame>(std::move(frame));
  }

  // Wrap a CVPixelBufferRef / AHardwareBuffer* pointer (typed as void* via
  // BigInt on the JS side) into one of our VideoFrames. The native side
  // CFRetains / acquires so the caller can release immediately.
  std::shared_ptr<VideoFrame> createVideoFrameFromNativeBuffer(void *pointer) {
    ensureSessionActive();
    auto handle = _platformContext->wrapNativeBuffer(pointer);
    return std::make_shared<VideoFrame>(std::move(handle));
  }

  std::shared_ptr<VideoPlayer>
  createVideoPlayer(std::string path, std::optional<std::string> pixelFormat) {
    ensureSessionActive();
    auto format = (pixelFormat && pixelFormat.value() == "nv12")
                      ? VideoPixelFormat::NV12
                      : VideoPixelFormat::BGRA8;
    auto impl = _platformContext->createVideoPlayer(path, format);
    return std::make_shared<VideoPlayer>(std::move(impl));
  }

  std::string writeTestVideoFile() {
    ensureSessionActive();
    return _platformContext->writeTestVideoFile();
  }

  std::shared_ptr<Canvas> getNativeSurface(int contextId) {
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    if (!_sessionState || !_sessionState->isActive()) {
      return std::make_shared<Canvas>(nullptr, 0, 0);
    }
    auto info = registry.getSurfaceInfo(_sessionState->id(), contextId);
    if (info == nullptr) {
      return std::make_shared<Canvas>(nullptr, 0, 0);
    }
    auto nativeInfo = info->getNativeInfo();
    return std::make_shared<Canvas>(nativeInfo.nativeSurface, nativeInfo.width,
                                    nativeInfo.height,
                                    std::move(nativeInfo.nativeSurfaceOwner));
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "fabric", &RNWebGPU::getFabric);
    installGetter(runtime, prototype, "sessionId", &RNWebGPU::getSessionId);
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
  }

private:
  static constexpr const char *kInactiveSessionError =
      "WebGPU runtime session is no longer active";

  void ensureSessionActive() const {
    if (!_sessionState || !_sessionState->isActive()) {
      throw std::runtime_error(kInactiveSessionError);
    }
  }

  static void settleImageBitmapSuccess(
      const std::shared_ptr<facebook::react::CallInvoker> &callInvoker,
      const std::shared_ptr<Promise> &promise,
      const std::shared_ptr<RNWebGPUSessionState> &sessionState,
      ImageData imageData) noexcept {
    try {
      callInvoker->invokeAsync(
          [promise, sessionState,
           imageData = std::move(imageData)](jsi::Runtime &runtime) mutable {
            if (!promise->belongsToRuntime(&runtime)) {
              return;
            }
            if (!sessionState || !sessionState->isActive()) {
              rejectPromiseNoexcept(promise, kInactiveSessionError);
              return;
            }
            try {
              auto imageBitmap =
                  std::make_shared<ImageBitmap>(std::move(imageData));
              promise->resolveWith([imageBitmap](jsi::Runtime &owningRuntime) {
                return JSIConverter<std::shared_ptr<ImageBitmap>>::toJSI(
                    owningRuntime, imageBitmap);
              });
            } catch (const std::exception &error) {
              rejectPromiseNoexcept(promise, error.what());
            } catch (...) {
              rejectPromiseNoexcept(promise, "Failed to create ImageBitmap");
            }
          });
    } catch (...) {
      settleImageBitmapError(callInvoker, promise, sessionState,
                             "Failed to dispatch ImageBitmap result");
    }
  }

  static void settleImageBitmapError(
      const std::shared_ptr<facebook::react::CallInvoker> &callInvoker,
      const std::shared_ptr<Promise> &promise,
      const std::shared_ptr<RNWebGPUSessionState> &sessionState,
      std::string error) noexcept {
    try {
      callInvoker->invokeAsync(
          [promise, sessionState,
           error = std::move(error)](jsi::Runtime &runtime) mutable {
            if (!promise->belongsToRuntime(&runtime)) {
              return;
            }
            if (!sessionState || !sessionState->isActive()) {
              rejectPromiseNoexcept(promise, kInactiveSessionError);
              return;
            }
            rejectPromiseNoexcept(promise, std::move(error));
          });
    } catch (...) {
      // No background thread may destroy the Promise's JSI functions. Its
      // PromiseRuntimeContext remains the final cleanup fallback if dispatch
      // itself cannot allocate a callback.
    }
  }

  static void rejectPromiseNoexcept(const std::shared_ptr<Promise> &promise,
                                    std::string error) noexcept {
    try {
      promise->reject(std::move(error));
    } catch (...) {
      // A CallInvoker callback must not propagate through the runtime loop.
    }
  }

  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
  std::shared_ptr<facebook::react::CallInvoker> _callInvoker;
  std::shared_ptr<RNWebGPUSessionState> _sessionState;
  const jsi::Runtime *_owningRuntime;
};

} // namespace rnwgpu
