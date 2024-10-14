#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "Canvas.h"
#include "GPU.h"
#include "GPUCanvasConfiguration.h"
#include "GPUTexture.h"
#include "SurfaceRegistry.h"

#ifdef __APPLE__

namespace dawn {
namespace native {
namespace metal {

// See
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/dawn/include/dawn/native/MetalBackend.h;l=41
void WaitForCommandsToBeScheduled(WGPUDevice device);

} // namespace metal
} // namespace native
} // namespace dawn

#endif

namespace rnwgpu {

namespace m = margelo;

class GPUCanvasContext : public m::HybridObject {
public:
  GPUCanvasContext(std::shared_ptr<GPU> gpu, int contextId, int width,
                   int height)
      : HybridObject("GPUCanvasContext"), _gpu(std::move(gpu)) {
    _canvas = std::make_shared<Canvas>(contextId, nullptr, width, height);
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
