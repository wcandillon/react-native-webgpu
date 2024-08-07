#pragma once

#include <memory>

#include "GPU.h"
#include "GPUCanvasContext.h"

namespace rnwgpu {

namespace m = margelo;

class Navigator : public m::HybridObject {
public:
  explicit Navigator(std::shared_ptr<GPU> gpu,
                     std::shared_ptr<PlatformContext> platformContext)
      : HybridObject("Navigator"), _gpu(gpu),
        _platformContext(platformContext) {}

  std::shared_ptr<GPU> getGPU() { return _gpu; }

  std::shared_ptr<GPUCanvasContext>
  MakeWebGPUCanvasContext(uint64_t nativeSurface, float width, float height) {
    auto surface = _platformContext->makeSurface(_gpu->get(),
        reinterpret_cast<void *>(nativeSurface), width, height);
    if (surface == nullptr) {
        throw std::runtime_error("null surface");
    }
    auto ctx = std::make_shared<GPUCanvasContext>(surface, width, height);
    Logger::logToConsole("width: %f", ctx->getCanvas()->getWidth());
    return ctx;
  }

  void loadHybridMethods() override {
    registerHybridGetter("gpu", &Navigator::getGPU, this);
    registerHybridMethod("MakeWebGPUCanvasContext",
                         &Navigator::MakeWebGPUCanvasContext, this);
  }

private:
  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
};

} // namespace rnwgpu
