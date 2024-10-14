#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "RNWebGPUManager.h"

namespace rnwgpu {

void GPUCanvasContext::configure(
    std::shared_ptr<GPUCanvasConfiguration> configuration) {
  Convertor conv;
  wgpu::SurfaceConfiguration surfaceConfiguration;
  surfaceConfiguration.device = configuration->device->get();
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
  _surfaceInfo->configure(surfaceConfiguration);
}

void GPUCanvasContext::unconfigure() {}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  auto prevSize = _surfaceInfo->getSize();
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();
  auto sizeHasChanged = prevSize.width != width || prevSize.height != height;
  if (sizeHasChanged) {
    _surfaceInfo->reconfigure(width, height);
  }
  auto texture = _surfaceInfo->getCurrentTexture();
  return std::make_shared<GPUTexture>(texture, "");
}

void GPUCanvasContext::present() {
#ifdef __APPLE__
  dawn::native::metal::WaitForCommandsToBeScheduled(
      _surfaceInfo->getDevice().Get());
#endif
  auto size = _surfaceInfo->getSize();
  _canvas->setClientWidth(size.width);
  _canvas->setClientHeight(size.height);
  _surfaceInfo->present();
}

} // namespace rnwgpu
