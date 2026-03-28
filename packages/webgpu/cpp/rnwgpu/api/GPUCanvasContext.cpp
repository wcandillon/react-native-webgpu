#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "RNWebGPUManager.h"
#include <memory>

namespace rnwgpu {

GPUCanvasContext::GPUCanvasContext(std::shared_ptr<GPU> gpu, int contextId,
  float width, float height, float pixelRatio)
    : NativeObject(CLASS_NAME), _gpu(std::move(gpu)), _contextId(contextId) {

  _canvas = std::make_shared<Canvas>(nullptr, width, height, pixelRatio);
  auto &registry = SurfaceRegistry::getInstance();
  _bridge = registry.getSurfaceInfoOrCreate(contextId, _gpu->get());
}

void GPUCanvasContext::configure(std::shared_ptr<GPUCanvasConfiguration> configuration) {
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
  _bridge->configure(surfaceConfiguration);
}

void GPUCanvasContext::unconfigure() {}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  auto texture = _bridge->getCurrentTexture(_canvas->getWidth(), _canvas->getHeight());
  if (!texture) {
    return nullptr;
  }
  auto result = std::make_shared<GPUTexture>(texture, "");
  result->setGPULock(getGPULock());
  _startedFrame = true;
  return result;
}

void GPUCanvasContext::present() {
  if (_startedFrame) {
    _bridge->present();
  }
  _startedFrame = false;
}

} // namespace rnwgpu
