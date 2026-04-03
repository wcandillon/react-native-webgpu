#include "GPUTexture.h"

#include <memory>

#include "Convertors.h"

namespace rnwgpu {

void GPUTexture::destroy() { _instance.Destroy(); }

std::shared_ptr<GPUTextureView> GPUTexture::createView(
    std::optional<std::shared_ptr<GPUTextureViewDescriptor>> descriptor) {
  wgpu::TextureViewDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUTextureView.createView(): couldn't access "
                             "GPUTextureViewDescriptor");
  }
  auto view = _instance.CreateView(&desc);
  return std::make_shared<GPUTextureView>(
      view,
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "");
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

double GPUTexture::getUsage() {
  return static_cast<double>(_instance.GetUsage());
}

size_t GPUTexture::_computeMemoryPressure() {
  uint32_t width = getWidth();
  uint32_t height = getHeight();
  uint32_t depthOrArrayLayers = getDepthOrArrayLayers();
  uint32_t mipLevelCount = getMipLevelCount();
  uint32_t sampleCount = getSampleCount();

  size_t bytesPerPixel = 4;
  wgpu::TextureFormat format = getFormat();
  switch (format) {
  case wgpu::TextureFormat::R8Unorm:
  case wgpu::TextureFormat::R8Snorm:
  case wgpu::TextureFormat::R8Uint:
  case wgpu::TextureFormat::R8Sint:
    bytesPerPixel = 1;
    break;
  case wgpu::TextureFormat::R16Uint:
  case wgpu::TextureFormat::R16Sint:
  case wgpu::TextureFormat::R16Float:
  case wgpu::TextureFormat::RG8Unorm:
  case wgpu::TextureFormat::RG8Snorm:
  case wgpu::TextureFormat::RG8Uint:
  case wgpu::TextureFormat::RG8Sint:
    bytesPerPixel = 2;
    break;
  case wgpu::TextureFormat::RGBA8Unorm:
  case wgpu::TextureFormat::RGBA8UnormSrgb:
  case wgpu::TextureFormat::RGBA8Snorm:
  case wgpu::TextureFormat::RGBA8Uint:
  case wgpu::TextureFormat::RGBA8Sint:
  case wgpu::TextureFormat::BGRA8Unorm:
  case wgpu::TextureFormat::BGRA8UnormSrgb:
  case wgpu::TextureFormat::RGB10A2Unorm:
  case wgpu::TextureFormat::R32Float:
  case wgpu::TextureFormat::R32Uint:
  case wgpu::TextureFormat::R32Sint:
  case wgpu::TextureFormat::RG16Uint:
  case wgpu::TextureFormat::RG16Sint:
  case wgpu::TextureFormat::RG16Float:
    bytesPerPixel = 4;
    break;
  case wgpu::TextureFormat::RG32Float:
  case wgpu::TextureFormat::RG32Uint:
  case wgpu::TextureFormat::RG32Sint:
  case wgpu::TextureFormat::RGBA16Uint:
  case wgpu::TextureFormat::RGBA16Sint:
  case wgpu::TextureFormat::RGBA16Float:
    bytesPerPixel = 8;
    break;
  case wgpu::TextureFormat::RGBA32Float:
  case wgpu::TextureFormat::RGBA32Uint:
  case wgpu::TextureFormat::RGBA32Sint:
    bytesPerPixel = 16;
    break;
  default:
    bytesPerPixel = 4;
    break;
  }

  size_t totalMemory = 0;
  for (uint32_t mip = 0; mip < mipLevelCount; ++mip) {
    uint32_t mipWidth = std::max(1u, width >> mip);
    uint32_t mipHeight = std::max(1u, height >> mip);
    totalMemory += static_cast<size_t>(mipWidth) * mipHeight *
                   depthOrArrayLayers * bytesPerPixel * sampleCount;
  }

  return totalMemory;
}

} // namespace rnwgpu
