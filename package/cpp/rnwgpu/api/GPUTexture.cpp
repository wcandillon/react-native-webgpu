#include "GPUTexture.h"

namespace rnwgpu {

void GPUTexture::destroy() { _instance.Destroy(); }


std::shared_ptr<GPUTextureView>
GPUTexture::createView(std::shared_ptr<GPUTextureViewDescriptor> descriptor) {
  return std::make_shared<GPUTextureView>(_instance.CreateView(descriptor->getInstance()), descriptor->label);
}

uint32_t GPUTexture::getWidth() { return _instance.GetWidth(); }

uint32_t GPUTexture::getHeight() { return _instance.GetHeight(); }

uint32_t GPUTexture::getDepthOrArrayLayers() {
  return _instance.GetDepthOrArrayLayers();
}

uint32_t GPUTexture::getMipLevelCount() { return _instance.GetMipLevelCount(); }

uint32_t GPUTexture::getSampleCount() { return _instance.GetSampleCount(); }

wgpu::TextureDimension GPUTexture::getDimension() {
  return _instance.GetDimension();
}

wgpu::TextureFormat GPUTexture::getFormat() { return _instance.GetFormat(); }

uint32_t GPUTexture::getUsage() {
  return static_cast<double>(_instance.GetUsage());
}

} // namespace rnwgpu
