#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"

#include "Canvas.h"
#include "GPU.h"
#include "GPUCanvasConfiguration.h"
#include "GPUTexture.h"
#include "SurfaceRegistry.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCanvasContext : public m::HybridObject {
public:
  GPUCanvasContext(std::shared_ptr<GPU> gpu, int contextId, int width,
                   int height)
      : HybridObject("GPUCanvasContext"), _gpu(std::move(gpu)) {
    _canvas = std::make_shared<Canvas>(nullptr, width, height);
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    _surfaceInfo =
        registry.getSurfaceInfoOrCreate(contextId, _gpu->get(), width, height);
  }

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<Canvas> getCanvas() { return _canvas; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCanvasContext::getBrand, this);
    registerHybridGetter("canvas", &GPUCanvasContext::getCanvas, this);
    registerHybridMethod("configure", &GPUCanvasContext::configure, this);
    registerHybridMethod("unconfigure", &GPUCanvasContext::unconfigure, this);
    registerHybridMethod("getCurrentTexture",
                         &GPUCanvasContext::getCurrentTexture, this);
    registerHybridMethod("present", &GPUCanvasContext::present, this);
  }

  size_t getMemoryPressure() override {
    int width = 0;
    int height = 0;

    if (_surfaceInfo) {
      auto size = _surfaceInfo->getSize();
      width = size.width;
      height = size.height;
    }

    if (_canvas) {
      width = std::max(width, _canvas->getWidth());
      height = std::max(height, _canvas->getHeight());
    }

    if (width <= 0 || height <= 0) {
      return 4 * 1024 * 1024; // default to 4MB when size is unknown
    }

    constexpr size_t kBytesPerPixel = 4; // RGBA8 fallback
    constexpr size_t kFloor = 4 * 1024 * 1024;
    size_t estimated = static_cast<size_t>(width) *
                       static_cast<size_t>(height) * kBytesPerPixel;
    return std::max(estimated, kFloor);
  }

  // TODO: is this ok?
  inline const wgpu::Surface get() { return nullptr; }
  void configure(std::shared_ptr<GPUCanvasConfiguration> configuration);
  void unconfigure();
  std::shared_ptr<GPUTexture> getCurrentTexture();
  void present();

private:
  std::shared_ptr<Canvas> _canvas;
  std::shared_ptr<SurfaceInfo> _surfaceInfo;
  std::shared_ptr<GPU> _gpu;
};

} // namespace rnwgpu
