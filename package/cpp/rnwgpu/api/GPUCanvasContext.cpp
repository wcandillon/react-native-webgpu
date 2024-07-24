#include <RNWebGPUManager.h>
#include "GPUCanvasContext.h"
#include "Convertors.h"

namespace rnwgpu {

void GPUCanvasContext::configure(std::shared_ptr<GPUCanvasConfiguration> configuration) {
 wgpu::SurfaceConfiguration surfaceConfiguration;
 Convertor conv;
 if (!conv(surfaceConfiguration, configuration)) {
   throw std::runtime_error("Error with GPUTextureDescriptor");
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
