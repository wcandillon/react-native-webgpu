#include "RNWebGPUManager.h"
#include "GPUCanvasContext.h"
#include "Convertors.h"

namespace rnwgpu {

void GPUCanvasContext::configure(std::shared_ptr<GPUCanvasConfiguration> configuration) {
  Convertor conv;
  wgpu::SurfaceConfiguration surfaceConfiguration;
  surfaceConfiguration.device = configuration->device->get();
  if (configuration->viewFormats.has_value()) {
    if (!conv(
        surfaceConfiguration.viewFormats,
        surfaceConfiguration.viewFormatCount,
        configuration->viewFormats.value()
      )) {
      throw std::runtime_error("Error with SurfaceConfiguration");
    }
  }
  if (!conv(surfaceConfiguration.format, configuration->format)) {
    throw std::runtime_error("Error with SurfaceConfiguration");
  }
  if (!conv(surfaceConfiguration.usage, configuration->usage)) {
    throw std::runtime_error("Error with SurfaceConfiguration");
  }
  surfaceConfiguration.width = _width;
  surfaceConfiguration.height = _height;
  _instance.Configure(&surfaceConfiguration);
}

void GPUCanvasContext::unconfigure() {
  _instance.Unconfigure();
}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  wgpu::SurfaceTexture surfaceTexture;
  _instance.GetCurrentTexture(&surfaceTexture);
  auto texture = surfaceTexture.texture;
  return std::make_shared<GPUTexture>(texture, _label);
}

void GPUCanvasContext::present() {
  _instance.Present();
}

} // namespace rnwgpu
