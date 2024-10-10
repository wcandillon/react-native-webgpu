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
   // Are we onscreen now?
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto infoVal = registry.getSurfaceMaybe(_contextId);
  if (infoVal.has_value()) {
    auto info = infoVal.value();
    if (info.surface) {
      _instance = info.surface;   
      _offscreenSurface = nullptr;
    }
  }

  if (_instance) {
    _instance.Configure(&surfaceConfiguration);
  } else {
    _offscreenSurface->configure(_surfaceConfiguration);
    // Add texture to the surface registry, when the native surface is
    // available, we will copy its content there This only makes sense if the on
    // screen native surface is not available yet
    registry.configureOffscreenSurface(_contextId, _gpu->get(), _offscreenSurface->getCurrentTexture(), _surfaceConfiguration);
  }
}

void GPUCanvasContext::unconfigure() {
  if (_instance) {
    _instance.Unconfigure();
  } else if (_offscreenSurface) {
    _offscreenSurface = nullptr;
  }
}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();

  auto prevWidth = _surfaceConfiguration.width;
  auto prevHeight = _surfaceConfiguration.height;
  auto sizeHasChanged = prevWidth != width || prevHeight != height;
  _surfaceConfiguration.width = width;
  _surfaceConfiguration.height = height;
  // Get onscreen texture
  if (_instance) {
    // Did the surface resize?
    if (sizeHasChanged) {
      _instance.Configure(&_surfaceConfiguration);
    }
    // Get onscreen texture
    wgpu::SurfaceTexture surfaceTexture;
    _instance.GetCurrentTexture(&surfaceTexture);
    auto texture = surfaceTexture.texture;
    if (texture == nullptr) {
      throw std::runtime_error("Couldn't get current texture");
    }
    return std::make_shared<GPUTexture>(texture, "");
  } else {
    // Did the surface resize?
    if (sizeHasChanged) {
      _offscreenSurface->configure(_surfaceConfiguration);
    }
    // Get offscreen texture
    auto tex = _offscreenSurface->getCurrentTexture();
    return std::make_shared<GPUTexture>(tex, "");
  }
}

void GPUCanvasContext::present() {
#ifdef __APPLE__
  dawn::native::metal::WaitForCommandsToBeScheduled(_device.Get());
#endif
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurface(_contextId);
  // We are starting a new frame, this is a good time to update the client
  _canvas->setClientWidth(info.width);
  _canvas->setClientHeight(info.height);
  if (_instance) {
    _instance.Present();
  } else {
    // Are we onscreen now?
    if (info.surface) {
      _instance = info.surface;
      _offscreenSurface = nullptr;
    }
  }
}

} // namespace rnwgpu
