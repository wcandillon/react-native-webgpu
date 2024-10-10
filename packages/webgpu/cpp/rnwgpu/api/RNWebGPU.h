#pragma once

#include <memory>
#include <string>

#include "Canvas.h"
#include "GPU.h"
#include "GPUCanvasContext.h"
#include "ImageBitmap.h"
#include "PlatformContext.h"

namespace rnwgpu {

namespace m = margelo;

struct Blob {
  std::string blobId;
  double size;
  double offset;
  std::string type;
  std::string name;
};

class RNWebGPU : public m::HybridObject {
public:
  explicit RNWebGPU(std::shared_ptr<GPU> gpu,
                    std::shared_ptr<PlatformContext> platformContext)
      : HybridObject("RNWebGPU"), _gpu(gpu), _platformContext(platformContext) {
  }

  std::shared_ptr<GPU> getGPU() { return _gpu; }

  bool getFabric() {
#ifdef RCT_NEW_ARCH_ENABLED
    return true;
#else
    return false;
#endif
  }

  std::shared_ptr<GPUCanvasContext>
  MakeWebGPUCanvasContext(int contextId, float width, float height) {
    auto ctx =
        std::make_shared<GPUCanvasContext>(_gpu, contextId, width, height);
    return ctx;
  }

  std::shared_ptr<ImageBitmap> createImageBitmap(std::shared_ptr<Blob> blob) {
    auto imageData = _platformContext->createImageBitmap(
        blob->blobId, blob->offset, blob->size);
    auto imageBitmap = std::make_shared<ImageBitmap>(imageData);
    return imageBitmap;
  }

  std::shared_ptr<Canvas> getNativeSurface(int contextId) {
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto info = registry.getSurface(contextId);
    return std::make_shared<Canvas>(info.nativeSurface, info.width,
                                    info.height);
  }

  void loadHybridMethods() override {
    registerHybridGetter("fabric", &RNWebGPU::getFabric, this);
    registerHybridGetter("gpu", &RNWebGPU::getGPU, this);
    registerHybridMethod("createImageBitmap", &RNWebGPU::createImageBitmap,
                         this);
    registerHybridMethod("getNativeSurface", &RNWebGPU::getNativeSurface, this);
    registerHybridMethod("MakeWebGPUCanvasContext",
                         &RNWebGPU::MakeWebGPUCanvasContext, this);
  }

private:
  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::Blob>> {
  static std::shared_ptr<rnwgpu::Blob>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    if (!outOfBounds && arg.isObject()) {
      auto result = std::make_unique<rnwgpu::Blob>();
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
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::Blob> arg) {
    throw std::runtime_error("Invalid Blob::toJSI()");
  }
};

} // namespace margelo
