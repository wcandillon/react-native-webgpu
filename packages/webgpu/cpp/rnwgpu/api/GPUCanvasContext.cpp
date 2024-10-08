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
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();
  surfaceConfiguration.width = width;
  surfaceConfiguration.height = height;
  _surfaceConfiguration = surfaceConfiguration;
  _offscreenSurface->configure(_surfaceConfiguration);
  // Add texture to the surface registry, when the native surface is available,
  // we will copy its content there
  // This only makes sense if the on screen native surface is not available yet
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  SurfaceInfo info;
  info.width = width;
  info.height = height;
  info.texture = _offscreenSurface->getCurrentTexture();
  info.device = _device;
  info.gpu = _gpu->get();
  info.config = _surfaceConfiguration;
  registry.addIfEmptySurface(_contextId, info);
}

void GPUCanvasContext::unconfigure() {
  if (_instance) {
    _instance.Unconfigure();
  } else if (_offscreenSurface) {
    _offscreenSurface->unconfigure();
  }
}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();

  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  // TODO: delete Java_com_webgpu_WebGPUModule_createSurfaceContext (and on iOS)

  // 1. is a onscreen surface now available?
  if (_pristine && _instance == nullptr) {
    if (registry.hasOnScreenSurface(_contextId)) {
      auto info = registry.getSurface(_contextId);
      // if the native surface is available, but we didn't create the WGPU instance yet, do it now
      if (info.surface == nullptr) {
        info.surface = _platformContext->makeSurface(_gpu->get(), info.nativeSurface, width, height);
      }
      _instance = info.surface;

      _surfaceConfiguration.width = width;
      _surfaceConfiguration.height = height;
      _instance.Configure(&_surfaceConfiguration);
      // TODO: _offscreenSurface = nullptr; ?
    }
  }

  // 2. did the surface resize?
  auto prevWidth = _surfaceConfiguration.width;
  auto prevHeight = _surfaceConfiguration.height;
  auto sizeHasChanged = prevWidth != width || prevHeight != height;
  if (_instance && _pristine && sizeHasChanged) {
    _surfaceConfiguration.width = width;
    _surfaceConfiguration.height = height;
    _instance.Configure(&_surfaceConfiguration);
  }

  _pristine = false;
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
    // We update the client width/height for the next frame
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto info = registry.getSurface(_contextId);
    _canvas->setClientWidth(info.width);
    _canvas->setClientHeight(info.height);
  }
  _pristine = true;
}

} // namespace rnwgpu
