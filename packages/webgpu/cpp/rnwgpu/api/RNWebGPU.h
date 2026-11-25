#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(__ANDROID__)
#include <unistd.h>
#endif

#include "NativeObject.h"

#include "ArrayBuffer.h"
#include "Canvas.h"
#include "CreateNativeBufferFromPointerOptions.h"
#include "GPU.h"
#include "GPUCanvasContext.h"
#include "GPUDevice.h"
#include "GPUSharedFenceState.h"
#include "GPUTexture.h"
#include "ImageBitmap.h"
#include "NativeBufferViewRegistry.h"
#include "NativeFrameImport.h"
#include "PlatformContext.h"
#include "VideoFrame.h"
#include "VideoPlayer.h"
#include "WebGPURuntime.h"
#include "rnwgpu/async/RuntimeContext.h"

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

  // A canvas context presenting to a native window that is not a WebGPU
  // view, e.g. the input Surface of a MediaCodec encoder: an ANativeWindow*
  // (Android) or a CAMetalLayer* (Apple), passed as a BigInt. Unlike a view's,
  // the context owns its surface, outside of the SurfaceRegistry, and the
  // caller drives it from one thread at a time. The context retains the
  // window for as long as it lives; unconfigure() releases the surface (and
  // on Android the buffers the window handed to it) and configure() makes a
  // new one for the window, see the SurfaceInfo constructor.
  std::shared_ptr<GPUCanvasContext>
  MakeWebGPUCanvasContextFromNativeWindow(void *window, float width,
                                          float height) {
    if (window == nullptr) {
      throw std::runtime_error(
          "MakeWebGPUCanvasContextFromNativeWindow: expected a non-null "
          "ANativeWindow (Android) or CAMetalLayer (Apple) pointer (BigInt)");
    }
    if (!(std::isfinite(width) && std::isfinite(height) && width > 0 &&
          height > 0)) {
      throw std::runtime_error(
          "MakeWebGPUCanvasContextFromNativeWindow: expected a positive "
          "width and height");
    }
    auto &runtime = WebGPURuntime::shared();
    // Unlike a view's, this surface has no offscreen fallback worth having:
    // nothing would ever show up, so fail here.
    auto surface = runtime.makeSurface(window);
    if (surface == nullptr) {
      throw std::runtime_error(
          "MakeWebGPUCanvasContextFromNativeWindow: could not create a surface "
          "for the window; is it a valid ANativeWindow (Android) or "
          "CAMetalLayer (Apple)?");
    }
    auto info = SurfaceRegistry::getInstance().makeUnregisteredSurfaceInfo(
        runtime.instance(), static_cast<int>(width), static_cast<int>(height));
    // The window is retained last, once nothing left here can throw: the
    // SurfaceInfo owns the reference from here on. Adopted by the context's
    // configure() (see SurfaceInfo::configure).
    info->attachSurface(window, std::move(surface),
                        _platformContext->retainNativeWindow(window));
    return std::make_shared<GPUCanvasContext>(_gpu, std::move(info), width,
                                              height);
  }

  // Wrap an externally created WGPUDevice (passed as a BigInt pointer, e.g.
  // Skia's Graphite device from Skia.getNativeDevice()) in a GPUDevice.
  // AddRefs the handle, so the original owner keeps its reference. Only sound
  // because the process links a single Dawn copy. The device's async
  // callbacks are waited for on the instance that owns it (RuntimeContext is
  // per runtime and per instance), which is not necessarily the shared one:
  // WebGPURuntime adopts Skia's instance only when Skia's getter is reachable
  // as the instance is first used (on Android, only once librnskia.so is
  // loaded), and other exporters have their own.
  std::shared_ptr<GPUDevice> importDevice(jsi::Runtime &runtime,
                                          void *pointer) {
    if (pointer == nullptr) {
      throw std::runtime_error(
          "importDevice: expected a non-null WGPUDevice pointer (BigInt)");
    }
    auto raw = reinterpret_cast<WGPUDevice>(pointer);
    wgpuDeviceAddRef(raw);
    wgpu::Device device = wgpu::Device::Acquire(raw);
    auto ctx = async::RuntimeContext::getOrCreate(
        runtime, device.GetAdapter().GetInstance());
    return std::make_shared<GPUDevice>(device, ctx, "Imported Device",
                                       GPUDevice::Origin::Imported);
  }

  // Wrap an externally created WGPUTexture in a GPUTexture, TAKING OWNERSHIP
  // of one reference (unlike importDevice, which borrows): the returned
  // GPUTexture releases it when destroyed. Pair with producers that return a
  // +1 pointer, e.g. Skia.Image.MakeNativeTextureFromImage().
  std::shared_ptr<GPUTexture> adoptTexture(void *pointer) {
    if (pointer == nullptr) {
      throw std::runtime_error(
          "adoptTexture: expected a non-null WGPUTexture pointer (BigInt)");
    }
    wgpu::Texture texture =
        wgpu::Texture::Acquire(reinterpret_cast<WGPUTexture>(pointer));
    return std::make_shared<GPUTexture>(texture, "Adopted Texture");
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

    // Resolve the requested alpha representation from the ImageBitmapOptions.
    // The options bag is the second argument for createImageBitmap(source,
    // options) and the sixth for the crop-rect overload createImageBitmap(
    // source, sx, sy, sw, sh, options). premultiplyAlpha defaults to
    // "default", which (like "premultiply") stores premultiplied pixels; only
    // "none" keeps straight alpha. The other options (crop rect, resize,
    // imageOrientation, colorSpaceConversion) are not yet implemented natively.
    bool wantPremultiplied = true;
    const jsi::Value *optionsArg = nullptr;
    if (count >= 2 && args[1].isObject()) {
      optionsArg = &args[1];
    } else if (count >= 6 && args[5].isObject()) {
      optionsArg = &args[5];
    }
    if (optionsArg != nullptr) {
      auto options = optionsArg->getObject(runtime);
      if (options.hasProperty(runtime, "premultiplyAlpha")) {
        auto value = options.getProperty(runtime, "premultiplyAlpha");
        if (value.isString() &&
            value.getString(runtime).utf8(runtime) == "none") {
          wantPremultiplied = false;
        }
      }
    }

    // Bring the decoded pixels into the representation requested via
    // premultiplyAlpha before wrapping them in an ImageBitmap.
    auto toRequestedAlpha = [wantPremultiplied](ImageData &imageData) {
      if (imageData.premultiplied != wantPremultiplied) {
        convertAlpha(imageData.data.data(), imageData.data.size(),
                     imageData.premultiplied, wantPremultiplied);
        imageData.premultiplied = wantPremultiplied;
      }
    };

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
            runtime, [platformContext, callInvoker, toRequestedAlpha,
                      dataCopy = std::move(dataCopy)](
                         jsi::Runtime & /*runtime*/,
                         std::shared_ptr<Promise> promise) mutable {
              platformContext->createImageBitmapFromDataAsync(
                  dataCopy,
                  [callInvoker, promise,
                   toRequestedAlpha](ImageData imageData) {
                    toRequestedAlpha(imageData);
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
        [platformContext, callInvoker, toRequestedAlpha, blobId, offset,
         size](jsi::Runtime & /*runtime*/, std::shared_ptr<Promise> promise) {
          platformContext->createImageBitmapAsync(
              blobId, offset, size,
              [callInvoker, promise, toRequestedAlpha](ImageData imageData) {
                toRequestedAlpha(imageData);
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

  std::shared_ptr<VideoFrame>
  createTestVideoFrame(double width, double height,
                       std::optional<std::string> pixelFormat) {
    auto format = (pixelFormat && pixelFormat.value() == "nv12")
                      ? VideoPixelFormat::NV12
                      : VideoPixelFormat::BGRA8;
    auto frame = _platformContext->createTestVideoFrame(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), format);
    return std::make_shared<VideoFrame>(std::move(frame));
  }

  // Wrap a CVPixelBufferRef / AHardwareBuffer* pointer (typed as void* via
  // BigInt on the JS side) into one of our VideoFrames. The native side
  // CFRetains / acquires so the caller can release immediately. The members
  // of `options.colorSpace` replace what the buffer says about its own.
  std::shared_ptr<VideoFrame> createNativeBufferFromPointer(
      void *pointer,
      std::optional<std::shared_ptr<CreateNativeBufferFromPointerOptions>>
          options) {
    auto handle = _platformContext->wrapNativeBuffer(pointer);
    if (options && (*options)->colorSpace) {
      const auto &given = *(*options)->colorSpace;
      auto &reported = handle.colorSpace;
      if (given.matrix) {
        reported.matrix = given.matrix;
      }
      if (given.range) {
        reported.range = given.range;
      }
      if (given.primaries) {
        reported.primaries = given.primaries;
      }
      if (given.transfer) {
        reported.transfer = given.transfer;
      }
    }
    return std::make_shared<VideoFrame>(std::move(handle));
  }

  // createNativeBuffer(width, height, { format?, colorSpace? }): an empty
  // native buffer (AHardwareBuffer / IOSurface) to render into through
  // importSharedTextureMemory() and display with presentNativeBuffer().
  // `format` defaults to the preferred canvas format of the platform
  // (rgba8unorm on Android, bgra8unorm on Apple platforms); rgba16float works
  // on both (more precision, not extended range: NativeBufferView clamps to
  // [0, 1]). `colorSpace` is "srgb" (the default) or "display-p3".
  jsi::Value createNativeBuffer(jsi::Runtime &runtime,
                                const jsi::Value & /*thisVal*/,
                                const jsi::Value *args, size_t count) {
    auto dimension = [&](size_t index, const char *name) {
      if (count <= index || !args[index].isNumber()) {
        throw jsi::JSError(runtime, std::string("createNativeBuffer: ") + name +
                                        " must be a number");
      }
      const double value = args[index].asNumber();
      if (!(value >= 1 && value <= 16384) || value != std::floor(value)) {
        throw jsi::JSError(runtime, std::string("createNativeBuffer: ") + name +
                                        " must be an integer in [1, 16384]");
      }
      return static_cast<uint32_t>(value);
    };
    const uint32_t width = dimension(0, "width");
    const uint32_t height = dimension(1, "height");
#if defined(__APPLE__)
    wgpu::TextureFormat format = wgpu::TextureFormat::BGRA8Unorm;
#else
    wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm;
#endif
    bool displayP3 = false;
    if (count > 2 && args[2].isObject()) {
      auto options = args[2].getObject(runtime);
      auto formatValue = options.getProperty(runtime, "format");
      if (formatValue.isString()) {
        auto name = formatValue.getString(runtime).utf8(runtime);
        if (name == "rgba8unorm") {
          format = wgpu::TextureFormat::RGBA8Unorm;
        } else if (name == "bgra8unorm") {
          format = wgpu::TextureFormat::BGRA8Unorm;
        } else if (name == "rgba16float") {
          format = wgpu::TextureFormat::RGBA16Float;
        } else {
          throw jsi::JSError(
              runtime, "createNativeBuffer: unsupported format '" + name + "'");
        }
      }
      auto colorSpaceValue = options.getProperty(runtime, "colorSpace");
      if (colorSpaceValue.isString()) {
        auto name = colorSpaceValue.getString(runtime).utf8(runtime);
        if (name == "display-p3") {
          displayP3 = true;
        } else if (name != "srgb") {
          throw jsi::JSError(runtime,
                             "createNativeBuffer: unsupported colorSpace '" +
                                 name + "'");
        }
      }
    }
    VideoFrameHandle handle;
    try {
      handle = _platformContext->createNativeBuffer(width, height, format,
                                                    displayP3);
    } catch (const std::exception &e) {
      throw jsi::JSError(runtime, e.what());
    }
    return VideoFrame::create(runtime,
                              std::make_shared<VideoFrame>(std::move(handle)));
  }

  // The id of a NativeBufferView about to mount, unique for the life of the
  // process (see NativeBufferViewRegistry). Presents are accepted once the
  // component retains it.
  int allocateNativeBufferViewId() {
    return NativeBufferViewRegistry::getInstance().allocateViewId();
  }

  // The NativeBufferView component mounted (again, when React StrictMode
  // re-runs its effects).
  void retainNativeBufferView(int viewId) {
    NativeBufferViewRegistry::getInstance().retainFromJS(viewId);
  }

  // The NativeBufferView component unmounted.
  void releaseNativeBufferView(int viewId) {
    NativeBufferViewRegistry::getInstance().releaseFromJS(viewId);
  }

  // presentNativeBuffer(viewId, frame, fences?): makes `frame` the content of
  // the NativeBufferView `viewId` once the GPU work that wrote it is done, or
  // right away without fences. `fences` is what
  // GPUSharedTextureMemory.endAccess() returns ({ fences }), a list of
  // GPUSharedFenceState, or a single one; several are merged into one. On
  // Android it also waits for the frame's own acquire fence, if it has one (a
  // buffer the producer may still be writing). Returns immediately; the view
  // shows the buffer on the next vsync after that. The view takes its own
  // reference on the buffer, but that does not keep a buffer pool from reusing
  // it: release the frame right away only if nothing else will write the buffer
  // while it is displayed (see NativeBufferView.tsx).
  jsi::Value presentNativeBuffer(jsi::Runtime &runtime,
                                 const jsi::Value & /*thisVal*/,
                                 const jsi::Value *args, size_t count) {
    if (count < 2 || !args[0].isNumber() || !args[1].isObject()) {
      throw jsi::JSError(runtime, "presentNativeBuffer(viewId, frame, fence?): "
                                  "expected a view id and a NativeBuffer");
    }
    const int viewId = static_cast<int>(args[0].asNumber());
    auto frame = JSIConverter<std::shared_ptr<VideoFrame>>::fromJSI(
        runtime, args[1], false);
    if (!frame) {
      throw jsi::JSError(
          runtime, "presentNativeBuffer(): frame is not a NativeBuffer");
    }
    wgpu::SharedFence fence;
    uint64_t signaledValue = 0;
    if (count > 2 && args[2].isObject()) {
      // { fences } (an end-access state), an array, or one fence state.
      auto arg = args[2].getObject(runtime);
      jsi::Value list = jsi::Value::undefined();
      if (arg.isArray(runtime)) {
        list = jsi::Value(runtime, arg);
      } else if (arg.hasProperty(runtime, "fences")) {
        list = arg.getProperty(runtime, "fences");
      }
      std::vector<std::shared_ptr<GPUSharedFenceState>> states;
      if (list.isObject()) {
        states = JSIConverter<
            std::vector<std::shared_ptr<GPUSharedFenceState>>>::fromJSI(runtime,
                                                                        list,
                                                                        false);
      } else {
        states.push_back(
            JSIConverter<std::shared_ptr<GPUSharedFenceState>>::fromJSI(
                runtime, args[2], false));
      }
      std::vector<wgpu::SharedFence> fences;
      std::vector<uint64_t> values;
      wgpu::Device device;
      for (const auto &state : states) {
        if (!state || !state->fence) {
          continue;
        }
        auto native = state->fence->get();
        if (!native) {
          throw jsi::JSError(runtime,
                             "presentNativeBuffer(): a fence was destroyed");
        }
        device = state->fence->device();
        fences.push_back(std::move(native));
        values.push_back(state->signaledValue);
      }
      try {
        native_frame::mergeFences(device, fences, values, &fence,
                                  &signaledValue, "presentNativeBuffer");
      } catch (const std::exception &e) {
        throw jsi::JSError(runtime, e.what());
      }
    }
    std::shared_ptr<NativeBufferContent> content;
    try {
      content = NativeBufferContent::fromFrame(frame->handle());
    } catch (const std::exception &e) {
      throw jsi::JSError(runtime, e.what());
    }
    int acquireFenceFd = -1;
#if defined(__ANDROID__)
    // Duplicated: the frame closes its own fd when it is released, which may
    // be before the wait is over.
    if (frame->handle().acquireFenceFd >= 0) {
      acquireFenceFd = ::dup(frame->handle().acquireFenceFd);
      if (acquireFenceFd < 0) {
        throw jsi::JSError(runtime, "presentNativeBuffer(): could not "
                                    "duplicate the frame's acquire fence");
      }
    }
#endif
    NativeBufferViewRegistry::getInstance().present(
        viewId, std::move(content), std::move(fence), signaledValue,
        acquireFenceFd);
    return jsi::Value::undefined();
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

  // The id for a Canvas about to mount, unique for the life of the process
  // (see SurfaceRegistry::allocateContextId).
  int allocateContextId() {
    return rnwgpu::SurfaceRegistry::getInstance().allocateContextId();
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
  //   The SurfaceInfo stays reachable by id for as long as a GPUCanvasContext
  //   holds it, so a surface attaching after StrictMode's early cleanup still
  //   lands in the context the first effect run made.
  // The check-and-erase runs atomically under the registry lock
  // (removeSurfaceInfoIfDetached), serialized against the UI thread's
  // find-or-create + attach (SurfaceRegistry::attachSurface), so a concurrent
  // attach can never be orphaned by this removal.
  void destroyContext(int contextId) {
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    registry.removeSurfaceInfoIfDetached(contextId);
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "fabric", &RNWebGPU::getFabric);
    installGetter(runtime, prototype, "gpu", &RNWebGPU::getGPU);
    installMethod(runtime, prototype, "createImageBitmap",
                  &RNWebGPU::createImageBitmap);
    installMethod(runtime, prototype, "allocateContextId",
                  &RNWebGPU::allocateContextId);
    installMethod(runtime, prototype, "getNativeSurface",
                  &RNWebGPU::getNativeSurface);
    installMethod(runtime, prototype, "destroyContext",
                  &RNWebGPU::destroyContext);
    installMethod(runtime, prototype, "MakeWebGPUCanvasContext",
                  &RNWebGPU::MakeWebGPUCanvasContext);
    installMethod(runtime, prototype, "MakeWebGPUCanvasContextFromNativeWindow",
                  &RNWebGPU::MakeWebGPUCanvasContextFromNativeWindow);
    installMethod(runtime, prototype, "loadVideoFrame",
                  &RNWebGPU::loadVideoFrame);
    installMethod(runtime, prototype, "createTestVideoFrame",
                  &RNWebGPU::createTestVideoFrame);
    installMethod(runtime, prototype, "createNativeBufferFromPointer",
                  &RNWebGPU::createNativeBufferFromPointer);
    installMethodWithRuntime(runtime, prototype, "importDevice",
                             &RNWebGPU::importDevice);
    installMethod(runtime, prototype, "adoptTexture", &RNWebGPU::adoptTexture);
    installMethod(runtime, prototype, "createVideoPlayer",
                  &RNWebGPU::createVideoPlayer);
    installMethod(runtime, prototype, "createNativeBuffer",
                  &RNWebGPU::createNativeBuffer);
    installMethod(runtime, prototype, "allocateNativeBufferViewId",
                  &RNWebGPU::allocateNativeBufferViewId);
    installMethod(runtime, prototype, "retainNativeBufferView",
                  &RNWebGPU::retainNativeBufferView);
    installMethod(runtime, prototype, "releaseNativeBufferView",
                  &RNWebGPU::releaseNativeBufferView);
    installMethod(runtime, prototype, "presentNativeBuffer",
                  &RNWebGPU::presentNativeBuffer);
    installMethod(runtime, prototype, "writeTestVideoFile",
                  &RNWebGPU::writeTestVideoFile);
  }

private:
  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
  std::shared_ptr<facebook::react::CallInvoker> _callInvoker;
};

} // namespace rnwgpu
