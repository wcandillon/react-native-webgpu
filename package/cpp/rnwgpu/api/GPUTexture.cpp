#include "GPUTexture.h"

namespace rnwgpu {

void GPUTexture::destroy() { _instance.Destroy(); }

double GPUTexture::getWidth() {
  return _instance.GetWidth();
}

double GPUTexture::getHeight() {
  return _instance.GetHeight();
}

double GPUTexture::getDepthOrArrayLayers() {
  return _instance.GetDepthOrArrayLayers();
}

double GPUTexture::getMipLevelCount() {
  return _instance.GetMipLevelCount();
}

double GPUTexture::getSampleCount() {
  return _instance.GetSampleCount();
}

wgpu::TextureDimension GPUTexture::getDimension() {
  return _instance.GetDimension();
}

wgpu::TextureFormat GPUTexture::getFormat() {
  return _instance.GetFormat();
}

double GPUTexture::getUsage() {
  return static_cast<double>(_instance.GetUsage());
}

} // namespace rnwgpu
