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
  _instance.Configure(&surfaceConfiguration);
}

void GPUCanvasContext::unconfigure() {
  _instance.Unconfigure();
}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  // we need to reconfigure if the size of the canvas or the surface has changed
  auto tex = _offscreenSurface->getCurrentTexture();
  return std::make_shared<GPUTexture>(tex, "offscreen_texture");
}

void GPUCanvasContext::present() {
  if (_instance) {
#ifdef __APPLE__
  dawn::native::metal::WaitForCommandsToBeScheduled(_device.Get());
#endif
  _instance.Present();
  }
}

} // namespace rnwgpu
