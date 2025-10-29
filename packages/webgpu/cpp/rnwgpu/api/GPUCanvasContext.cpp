#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "RNWebGPUManager.h"
#include <memory>

#ifdef __APPLE__
namespace dawn::native::metal {

void WaitForCommandsToBeScheduled(WGPUDevice device);

}
#endif

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
  if (!conv(surfaceConfiguration.usage, configuration->usage) ||
      !conv(surfaceConfiguration.format, configuration->format)) {
    throw std::runtime_error("Error with SurfaceConfiguration");
  }

#ifdef __APPLE__
  surfaceConfiguration.alphaMode = configuration->alphaMode;
#endif
  surfaceConfiguration.presentMode = wgpu::PresentMode::Fifo;
  _surfaceInfo->configure(surfaceConfiguration);
}

void GPUCanvasContext::unconfigure() {}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  auto prevSize = _surfaceInfo->getConfig();
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
  auto request = _surfaceInfo->takePendingPresent();
  if (!request.has_value()) {
    return;
  }
#ifdef __APPLE__
  dawn::native::metal::WaitForCommandsToBeScheduled(
      request->device.Get());
#endif
  _canvas->setClientWidth(request->width);
  _canvas->setClientHeight(request->height);
  request->surface.Present();
}

} // namespace rnwgpu
