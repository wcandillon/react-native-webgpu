#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "RNWebGPUManager.h"

#include <android/native_window.h>

namespace rnwgpu {

void GPUCanvasContext::configure(
    std::shared_ptr<GPUCanvasConfiguration> configuration) {
  Convertor conv;
  wgpu::SurfaceConfiguration surfaceConfiguration;
  surfaceConfiguration.device = configuration->device->get();
  _device = configuration->device->get();
  if (configuration->viewFormats.has_value()) {
    if (!conv(surfaceConfiguration.viewFormats,
              surfaceConfiguration.viewFormatCount,
              configuration->viewFormats.value())) {
      throw std::runtime_error("Error with SurfaceConfiguration");
    }
  }
  if (!conv(surfaceConfiguration.format, configuration->format)) {
    throw std::runtime_error("Error with SurfaceConfiguration");
  }
  if (!conv(surfaceConfiguration.usage, configuration->usage)) {
    throw std::runtime_error("Error with SurfaceConfiguration");
  }
  surfaceConfiguration.width = _canvas->getWidth();
  surfaceConfiguration.height = _canvas->getHeight();
  _surfaceConfiguration = surfaceConfiguration;
  _offscreenSurface->configure(_surfaceConfiguration);
}

void GPUCanvasContext::unconfigure() { _offscreenSurface->unconfigure(); }

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  // TODO: use Platform Context?
  // TODO: flush the content of the offscreen surface
  // we need to reconfigure if the size of the canvas or the surface has changed

  if (_pristine && _instance == nullptr) {
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto info = registry.getSurface(_contextId);
    if (info != nullptr) {
      wgpu::SurfaceDescriptorFromAndroidNativeWindow androidSurfaceDesc;
      androidSurfaceDesc.window =
          reinterpret_cast<ANativeWindow *>(info->surface);
      wgpu::SurfaceDescriptor surfaceDescriptor;
      surfaceDescriptor.nextInChain = &androidSurfaceDesc;
      _instance = _gpu->get().CreateSurface(&surfaceDescriptor);
      _instance.Configure(&_surfaceConfiguration);
      _pristine = false;
    }
  }
  if (_instance) {
    wgpu::SurfaceTexture surfaceTexture;
    _instance.GetCurrentTexture(&surfaceTexture);
    auto texture = surfaceTexture.texture;
    if (texture == nullptr) {
      throw std::runtime_error("Couldn't get current texture");
    }
    // Default canvas texture label is ""
    return std::make_shared<GPUTexture>(texture, "");
  }
  auto tex = _offscreenSurface->getCurrentTexture();
  return std::make_shared<GPUTexture>(tex, "offscreen_texture");
}

void GPUCanvasContext::present() {
#ifdef __APPLE__
  dawn::native::metal::WaitForCommandsToBeScheduled(_device.Get());
#endif
  if (_instance) {
    _instance.Present();
  }
  _pristine = true;
}

} // namespace rnwgpu
