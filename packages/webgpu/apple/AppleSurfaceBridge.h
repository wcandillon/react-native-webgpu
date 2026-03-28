#pragma once

#include "rnwgpu/SurfaceBridge.h"

#include <mutex>
#include <shared_mutex>

namespace rnwgpu {

class AppleSurfaceBridge : public SurfaceBridge, public std::enable_shared_from_this<AppleSurfaceBridge> {
public:
  AppleSurfaceBridge(GPUWithLock gpu);
  ~AppleSurfaceBridge() override {};

  // JS thread
  void configure(wgpu::SurfaceConfiguration &config) override;
  wgpu::Texture getCurrentTexture(int width, int height) override;
  bool present() override;

  // Called by the UI thread once from MetalView when it's ready.
  // The UI thread must hold the GPU device lock.
  void prepareToDisplay(void *nativeSurface, wgpu::Surface surface);

  NativeInfo getNativeInfo() override;
private:
  void _resizeSurface(int width, int height);
  void _doSurfaceConfiguration(int width, int height);

  wgpu::Instance _gpu;
  wgpu::SurfaceConfiguration _config;
  wgpu::Surface _surface = nullptr;

  // It's possible that the JS thread accesses the getCurrentTexture
  // before the UI thread attaches the native Metal layer. In this case
  // we render to an offscreen texture.
  wgpu::Texture _renderTargetTexture = nullptr;
  wgpu::Texture _presentedTexture = nullptr;
  void *_nativeSurface = nullptr;

  std::mutex _mutex;
  int _width;
  int _height;
};

} // namespace rnwgpu
