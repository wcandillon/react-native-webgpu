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
  auto width = _canvas->getWidth();
  auto height =  _canvas->getHeight();

  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  // TODO: flush the content of the offscreen surface
  // TODO: delete Java_com_webgpu_WebGPUModule_createSurfaceContext (and on iOS)

  // 1. is a surface no available?
  if (_pristine && _instance == nullptr) {
    if (_test) {
      _test = false;
    } else {
      auto info = registry.getSurface(_contextId);
      if (info != nullptr) {
        _instance = _platformContext->makeSurface(_gpu->get(), info->surface,
                                                  width, height);
        _surfaceConfiguration.width = width;
        _surfaceConfiguration.height = height;
        _instance.Configure(&_surfaceConfiguration);
        _offscreenSurface = nullptr;
        // TODO: flush offscreen content to onscreen
      }
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
    _canvas->setClientWidth(info->width);
    _canvas->setClientHeight(info->height);
  }
  _pristine = true;

}

} // namespace rnwgpu
