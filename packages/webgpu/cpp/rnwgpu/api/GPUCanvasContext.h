#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "Canvas.h"
#include "GPUCanvasConfiguration.h"
#include "GPUTexture.h"
#include "SurfaceRegistry.h"
#include "GPU.h"
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
  explicit GPUCanvasContext(wgpu::Surface instance,
                            std::shared_ptr<Canvas> canvas,
                            std::shared_ptr<PlatformContext> platformContext,
                            std::shared_ptr<GPU> gpu)
      : HybridObject("GPUCanvasContext"), _instance(instance), _canvas(canvas),
      _platformContext(platformContext), _gpu(gpu),
      _width(canvas->getWidth()), _height(canvas->getHeight()) {}

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
  void updateInstance(std::shared_ptr<Canvas> surface);
  void maybeFlushPendingTexture(std::shared_ptr<Canvas> surface);

private:
  wgpu::Surface _instance;
  wgpu::Device _device;
  std::shared_ptr<Canvas> _canvas;
  std::shared_ptr<GPUCanvasConfiguration> _lastConfig;
  float _width;
  float _height;
  wgpu::Texture _pendingTextureToFlush;
  bool _isPendingTextureToFlush = false;
  std::shared_ptr<PlatformContext> _platformContext;
  std::shared_ptr<GPU> _gpu;
  std::recursive_mutex _mutex;

  wgpu::Texture createMockTexture();
};

} // namespace rnwgpu
