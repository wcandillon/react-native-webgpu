#pragma once

#include <memory>

#include "GPU.h"
#include "GPUCanvasContext.h"
#include "PlatformContext.h"

namespace rnwgpu {

namespace m = margelo;

class RNWebGPU : public m::HybridObject {
public:
  explicit RNWebGPU(std::shared_ptr<GPU> gpu,
                     std::shared_ptr<PlatformContext> platformContext)
      : HybridObject("RNWebGPU"), _gpu(gpu),
        _platformContext(platformContext) {}

  std::shared_ptr<GPU> getGPU() { return _gpu; }

  std::shared_ptr<GPUCanvasContext>
  MakeWebGPUCanvasContext(std::shared_ptr<Canvas> canvas) {
    auto nativeSurface = canvas->getSurface();
    auto width = canvas->getWidth();
    auto height = canvas->getHeight();
    auto surface = _platformContext->makeSurface(
        _gpu->get(), reinterpret_cast<void *>(nativeSurface), width, height);
    if (surface == nullptr) {
      throw std::runtime_error("null surface");
    }
    auto ctx = std::make_shared<GPUCanvasContext>(surface, canvas);
    return ctx;
  }

  std::string DecodeToUTF8(std::shared_ptr<ArrayBuffer> buffer) {
    auto data = reinterpret_cast<const char *>(buffer->data());
    std::string result(data, buffer->size());
    return result;
  }

  void loadHybridMethods() override {
    registerHybridGetter("gpu", &RNWebGPU::getGPU, this);
    registerHybridMethod("DecodeToUTF8",
                         &RNWebGPU::DecodeToUTF8, this);
    registerHybridMethod("MakeWebGPUCanvasContext",
                         &RNWebGPU::MakeWebGPUCanvasContext, this);
  }

private:
  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
};

} // namespace rnwgpu
