#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "RNWebGPUManager.h"

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

void GPUCanvasContext::unconfigure() {
  if (_instance) {
    _instance.Unconfigure();
  } else if (_offscreenSurface) {
    _offscreenSurface->unconfigure();
  }
}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  // TODO: flush the content of the offscreen surface
  // TODO: delete Java_com_webgpu_WebGPUModule_createSurfaceContext (and on iOS)
  // 1. is a surface no available?
  if (_pristine && _instance == nullptr) {
    auto info = registry.getSurface(_contextId);
    if (info != nullptr) {
      _instance = _platformContext->makeSurface(_gpu->get(), info->surface, info->width, info->height);
     // _surfaceConfiguration.width = info->width;
     // _surfaceConfiguration.height = info->height;
      _instance.Configure(&_surfaceConfiguration);
      _offscreenSurface->unconfigure();
      _offscreenSurface = nullptr;
      _pristine = false;
    }
  }
  // 2. did the surface resize?
//  if (_instance) {
//      auto info = registry.getSurface(_contextId);
//      if (info != nullptr) {
//        if (info->width != _surfaceConfiguration.width || info->height != _surfaceConfiguration.height) {
//          _width = info->width;
//          _height = info->height;
//          _surfaceConfiguration.width = _width;
//          _surfaceConfiguration.height = _height;
//          _instance.Configure(&_surfaceConfiguration);
//        }
//      }
//  }
  // 3. get onscreen texture
  if (_instance) {
    wgpu::SurfaceTexture surfaceTexture;
    _instance.GetCurrentTexture(&surfaceTexture);
    auto texture = surfaceTexture.texture;
    if (texture == nullptr) {
      throw std::runtime_error("Couldn't get current texture");
    }
    return std::make_shared<GPUTexture>(texture, "");
  } else {
    // 4. get offscreen texture
    auto tex = _offscreenSurface->getCurrentTexture();
    return std::make_shared<GPUTexture>(tex, "");
  }
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
