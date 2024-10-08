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
  GPUCanvasContext(std::shared_ptr<GPU> gpu,
                   std::shared_ptr<PlatformContext> platformContext,
                   int contextId, float width, float height)
      : HybridObject("GPUCanvasContext"), _contextId(contextId),
        _gpu(std::move(gpu)), _platformContext(std::move(platformContext)) {
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
  bool _pristine = true;
  // for testing only;
  // TODO: to remove
  bool _test = true;
  int _contextId;

  std::shared_ptr<GPU> _gpu;
  std::shared_ptr<PlatformContext> _platformContext;

  wgpu::SurfaceConfiguration _surfaceConfiguration;
};

} // namespace rnwgpu
