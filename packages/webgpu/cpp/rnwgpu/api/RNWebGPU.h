#pragma once

#include <memory>
#include <string>

#include "NativeObject.h"

#include "Canvas.h"
#include "GPU.h"
#include "GPUCanvasContext.h"
#include "ImageBitmap.h"
#include "PlatformContext.h"

#include "JSIConverter.h"
#include "rnwgpu/async/AsyncRunner.h"
#include "rnwgpu/async/AsyncTaskHandle.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

struct Blob {
  std::string blobId;
  double size;
  double offset;
  std::string type;
  std::string name;
};

class RNWebGPU : public NativeObject<RNWebGPU> {
public:
  static constexpr const char *CLASS_NAME = "RNWebGPU";

  explicit RNWebGPU(std::shared_ptr<GPU> gpu,
                    std::shared_ptr<PlatformContext> platformContext,
                    std::shared_ptr<async::AsyncRunner> asyncRunner)
      : NativeObject(CLASS_NAME), _gpu(gpu), _platformContext(platformContext),
        _async(asyncRunner) {}

  std::shared_ptr<GPU> getGPU() { return _gpu; }

  bool getFabric() { return true; }

  std::shared_ptr<GPUCanvasContext>
  MakeWebGPUCanvasContext(int contextId, float width, float height) {
    auto ctx =
        std::make_shared<GPUCanvasContext>(_gpu, contextId, width, height);
    return ctx;
  }

  async::AsyncTaskHandle createImageBitmap(std::shared_ptr<Blob> blob) {
    auto platformContext = _platformContext;
    std::string blobId = blob->blobId;
    double offset = blob->offset;
    double size = blob->size;

    return _async->postTask(
        [platformContext, blobId, offset,
         size](const async::AsyncTaskHandle::ResolveFunction &resolve,
               const async::AsyncTaskHandle::RejectFunction &reject) {
          platformContext->createImageBitmapAsync(
              blobId, offset, size,
              [resolve](ImageData imageData) {
                auto imageBitmap = std::make_shared<ImageBitmap>(imageData);
                resolve([imageBitmap](jsi::Runtime &runtime) {
                  return JSIConverter<std::shared_ptr<ImageBitmap>>::toJSI(
                      runtime, imageBitmap);
                });
              },
              [reject](std::string error) { reject(error); });
        },
        false // Don't keep pumping - this is a one-shot async operation
    );
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
  }

private:
  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
  std::shared_ptr<async::AsyncRunner> _async;
};

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
        //        result->type = JSIConverter<std::string>::fromJSI(
        //            runtime, value.getProperty(runtime, "type"), false);
        //        result->name = JSIConverter<std::string>::fromJSI(
        //            runtime, value.getProperty(runtime, "name"), false);
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

} // namespace rnwgpu
