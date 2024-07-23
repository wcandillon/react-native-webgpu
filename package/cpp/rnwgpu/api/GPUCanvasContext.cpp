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
 surfaceConfiguration.width = 200;
 surfaceConfiguration.height = 200;
// _instance.Configure(&surfaceConfiguration);

 _device = surfaceConfiguration.device;
}

void GPUCanvasContext::unconfigure() {
  _instance.Unconfigure();
}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  wgpu::SwapChainDescriptor swapChainDesc;
  swapChainDesc.width = 200;
  swapChainDesc.height = 200;
  swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
  swapChainDesc.format = wgpu::TextureFormat::RGBA8Unorm;
  swapChainDesc.presentMode = wgpu::PresentMode::Fifo;
  auto surface = rnwgpu::RNWebGPUManager::surface;
  auto swapChain = _device.CreateSwapChain(*surface, &swapChainDesc);
  auto texture = swapChain.GetCurrentTexture();

  RNWebGPUManager::swapChain = std::make_shared<wgpu::SwapChain>(swapChain);
  RNWebGPUManager::texture = std::make_shared<wgpu::Texture>(texture);

  return std::make_shared<GPUTexture>(texture, "mleko");
}

void GPUCanvasContext::present() {
  RNWebGPUManager::swapChain->Present();
}

} // namespace rnwgpu
