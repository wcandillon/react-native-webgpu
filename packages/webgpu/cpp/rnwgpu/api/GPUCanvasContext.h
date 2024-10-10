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
#include "OffscreenSurface.h"
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
  // TODO: platformContext no necessary here
  GPUCanvasContext(std::shared_ptr<GPU> gpu, int contextId, int width,
                   int height)
      : HybridObject("GPUCanvasContext"), _contextId(contextId),
        _gpu(std::move(gpu)) {
    _canvas = std::make_shared<Canvas>(nullptr, width, height);
    _offscreenSurface = std::make_shared<OffscreenSurface>(_canvas);
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

  inline const wgpu::Surface get() { return _instance; }
  void configure(std::shared_ptr<GPUCanvasConfiguration> configuration);
  void unconfigure();
  std::shared_ptr<GPUTexture> getCurrentTexture();
  void present();

private:
  std::shared_ptr<OffscreenSurface> _offscreenSurface;
  wgpu::Surface _instance = nullptr;
  wgpu::Device _device;
  std::shared_ptr<Canvas> _canvas;
  int _contextId;

  std::shared_ptr<GPU> _gpu;
  // TODO: do we need this or can it be stored in the surface registry?
  wgpu::SurfaceConfiguration _surfaceConfiguration;
};

} // namespace rnwgpu
