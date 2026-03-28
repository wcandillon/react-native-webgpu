#pragma once

#include <android/native_window.h>

#include "rnwgpu/SurfaceBridge.h"

namespace rnwgpu {

class AndroidSurfaceBridge : public SurfaceBridge {
public:
  AndroidSurfaceBridge(GPUWithLock gpu);
  ~AndroidSurfaceBridge() override;

  // JS thread
  void configure(wgpu::SurfaceConfiguration &config) override;
  wgpu::Texture getCurrentTexture(int width, int height) override;
  bool present() override;

  // Android UI thread
  void switchToOnscreen(ANativeWindow *nativeWindow, wgpu::Surface surface);
  ANativeWindow *switchToOffscreen();

  // Read-only
  NativeInfo getNativeInfo() override;

private:
  void _copyToSurfaceAndPresent();

  wgpu::Instance _gpu;
  wgpu::SurfaceConfiguration _config;
  wgpu::Surface _surface = nullptr;
  wgpu::Texture _texture = nullptr;
  wgpu::Texture _presentedTexture = nullptr;
  ANativeWindow *_nativeWindow = nullptr;

  mutable std::mutex _mutex;
  int _width;
  int _height;
};

} // namespace rnwgpu
