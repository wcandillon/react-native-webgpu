#include "GPUTexture.h"

namespace rnwgpu {

void GPUTexture::destroy() { _instance.Destroy(); }

uint32_t GPUTexture::getWidth() {
  return _instance.GetWidth();
}

uint32_t GPUTexture::getHeight() {
  return _instance.GetHeight();
}

uint32_t GPUTexture::getDepthOrArrayLayers() {
  return _instance.GetDepthOrArrayLayers();
}

uint32_t GPUTexture::getMipLevelCount() {
  return _instance.GetMipLevelCount();
}

uint32_t GPUTexture::getSampleCount() {
  return _instance.GetSampleCount();
}

wgpu::TextureDimension GPUTexture::getDimension() {
  return _instance.GetDimension();
}

wgpu::TextureFormat GPUTexture::getFormat() {
  return _instance.GetFormat();
}

wgpu::TextureUsage GPUTexture::getUsage() {
  return _instance.GetUsage()
}

} // namespace rnwgpu