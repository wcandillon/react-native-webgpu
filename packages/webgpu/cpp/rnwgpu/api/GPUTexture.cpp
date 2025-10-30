#include "GPUTexture.h"

#include <cstddef>
#include <memory>

#include "Convertors.h"

namespace rnwgpu {

namespace {

constexpr size_t kBytesPerRowAlignment = 256;

struct FormatInfo {
  uint32_t blockWidth;
  uint32_t blockHeight;
  uint32_t blockDepth;
  size_t bytesPerBlock;
};

constexpr FormatInfo makeUncompressed(uint32_t bytesPerPixel) {
  return FormatInfo{1, 1, 1, bytesPerPixel};
}

constexpr FormatInfo makeCompressed(uint32_t blockWidth, uint32_t blockHeight, size_t bytesPerBlock) {
  return FormatInfo{blockWidth, blockHeight, 1, bytesPerBlock};
}

constexpr FormatInfo getFormatInfo(wgpu::TextureFormat format) {
  switch (format) {
  case wgpu::TextureFormat::R8Unorm:
  case wgpu::TextureFormat::R8Snorm:
  case wgpu::TextureFormat::R8Uint:
  case wgpu::TextureFormat::R8Sint:
    return makeUncompressed(1);
  case wgpu::TextureFormat::R16Uint:
  case wgpu::TextureFormat::R16Sint:
  case wgpu::TextureFormat::R16Float:
  case wgpu::TextureFormat::RG8Unorm:
  case wgpu::TextureFormat::RG8Snorm:
  case wgpu::TextureFormat::RG8Uint:
  case wgpu::TextureFormat::RG8Sint:
  case wgpu::TextureFormat::R8BG8Biplanar422Unorm:
    return makeUncompressed(2);
  case wgpu::TextureFormat::RGBA8Unorm:
  case wgpu::TextureFormat::RGBA8UnormSrgb:
  case wgpu::TextureFormat::RGBA8Snorm:
  case wgpu::TextureFormat::RGBA8Uint:
  case wgpu::TextureFormat::RGBA8Sint:
  case wgpu::TextureFormat::BGRA8Unorm:
  case wgpu::TextureFormat::BGRA8UnormSrgb:
  case wgpu::TextureFormat::RGB10A2Unorm:
  case wgpu::TextureFormat::RG11B10Ufloat:
  case wgpu::TextureFormat::RGB9E5Ufloat:
  case wgpu::TextureFormat::R32Float:
  case wgpu::TextureFormat::R32Uint:
  case wgpu::TextureFormat::R32Sint:
  case wgpu::TextureFormat::RG16Uint:
  case wgpu::TextureFormat::RG16Sint:
  case wgpu::TextureFormat::RG16Float:
  case wgpu::TextureFormat::Depth32Float:
  case wgpu::TextureFormat::Depth24Plus:
  case wgpu::TextureFormat::Depth24PlusStencil8:
    return makeUncompressed(4);
  case wgpu::TextureFormat::RG32Float:
  case wgpu::TextureFormat::RG32Uint:
  case wgpu::TextureFormat::RG32Sint:
  case wgpu::TextureFormat::RGBA16Uint:
  case wgpu::TextureFormat::RGBA16Sint:
  case wgpu::TextureFormat::RGBA16Float:
  case wgpu::TextureFormat::Depth32FloatStencil8:
    return makeUncompressed(8);
  case wgpu::TextureFormat::RGBA32Float:
  case wgpu::TextureFormat::RGBA32Uint:
  case wgpu::TextureFormat::RGBA32Sint:
    return makeUncompressed(16);
  case wgpu::TextureFormat::Depth16Unorm:
    return makeUncompressed(2);
  case wgpu::TextureFormat::Stencil8:
    return makeUncompressed(1);
  case wgpu::TextureFormat::BC1RGBAUnorm:
  case wgpu::TextureFormat::BC1RGBAUnormSrgb:
  case wgpu::TextureFormat::BC4RSnorm:
  case wgpu::TextureFormat::BC4RUnorm:
    return makeCompressed(4, 4, 8);
  case wgpu::TextureFormat::BC2RGBAUnorm:
  case wgpu::TextureFormat::BC2RGBAUnormSrgb:
  case wgpu::TextureFormat::BC3RGBAUnorm:
  case wgpu::TextureFormat::BC3RGBAUnormSrgb:
  case wgpu::TextureFormat::BC5RGSnorm:
  case wgpu::TextureFormat::BC5RGUnorm:
  case wgpu::TextureFormat::BC6HRGBFloat:
  case wgpu::TextureFormat::BC6HRGBUfloat:
  case wgpu::TextureFormat::BC7RGBAUnorm:
  case wgpu::TextureFormat::BC7RGBAUnormSrgb:
    return makeCompressed(4, 4, 16);
  case wgpu::TextureFormat::ETC2RGB8Unorm:
  case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
  case wgpu::TextureFormat::ETC2RGB8A1Unorm:
  case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
  case wgpu::TextureFormat::EACR11Unorm:
  case wgpu::TextureFormat::EACR11Snorm:
    return makeCompressed(4, 4, 8);
  case wgpu::TextureFormat::ETC2RGBA8Unorm:
  case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
  case wgpu::TextureFormat::EACRG11Unorm:
  case wgpu::TextureFormat::EACRG11Snorm:
    return makeCompressed(4, 4, 16);
  case wgpu::TextureFormat::ASTC4x4Unorm:
  case wgpu::TextureFormat::ASTC4x4UnormSrgb:
  case wgpu::TextureFormat::ASTC5x4Unorm:
  case wgpu::TextureFormat::ASTC5x4UnormSrgb:
  case wgpu::TextureFormat::ASTC5x5Unorm:
  case wgpu::TextureFormat::ASTC5x5UnormSrgb:
  case wgpu::TextureFormat::ASTC6x5Unorm:
  case wgpu::TextureFormat::ASTC6x5UnormSrgb:
  case wgpu::TextureFormat::ASTC6x6Unorm:
  case wgpu::TextureFormat::ASTC6x6UnormSrgb:
  case wgpu::TextureFormat::ASTC8x5Unorm:
  case wgpu::TextureFormat::ASTC8x5UnormSrgb:
  case wgpu::TextureFormat::ASTC8x6Unorm:
  case wgpu::TextureFormat::ASTC8x6UnormSrgb:
  case wgpu::TextureFormat::ASTC8x8Unorm:
  case wgpu::TextureFormat::ASTC8x8UnormSrgb:
  case wgpu::TextureFormat::ASTC10x5Unorm:
  case wgpu::TextureFormat::ASTC10x5UnormSrgb:
  case wgpu::TextureFormat::ASTC10x6Unorm:
  case wgpu::TextureFormat::ASTC10x6UnormSrgb:
  case wgpu::TextureFormat::ASTC10x8Unorm:
  case wgpu::TextureFormat::ASTC10x8UnormSrgb:
  case wgpu::TextureFormat::ASTC10x10Unorm:
  case wgpu::TextureFormat::ASTC10x10UnormSrgb:
  case wgpu::TextureFormat::ASTC12x10Unorm:
  case wgpu::TextureFormat::ASTC12x10UnormSrgb:
  case wgpu::TextureFormat::ASTC12x12Unorm:
  case wgpu::TextureFormat::ASTC12x12UnormSrgb:
    return FormatInfo{4, 4, 1, 16}; // ASTC blocks are always 128 bits per 4x4 texels
  default:
    return makeUncompressed(4);
  }
}

constexpr size_t alignTo(size_t value, size_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

} // namespace

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

size_t GPUTexture::getMemoryPressure() {
  const auto formatInfo = getFormatInfo(getFormat());
  const auto dimension = getDimension();

  uint32_t width = getWidth();
  uint32_t height = getHeight();
  uint32_t depthOrArrayLayers = getDepthOrArrayLayers();
  uint32_t mipLevelCount = getMipLevelCount();
  uint32_t sampleCount = std::max(1u, getSampleCount());

  bool is3DTexture = dimension == wgpu::TextureDimension::e3D;

  size_t totalMemory = 0;
  for (uint32_t mip = 0; mip < mipLevelCount; ++mip) {
    uint32_t mipWidth = std::max(1u, width >> mip);
    uint32_t mipHeight = std::max(1u, height >> mip);
    uint32_t mipLayers = is3DTexture ? std::max(1u, depthOrArrayLayers >> mip) : 1u;

    size_t blocksX = (mipWidth + formatInfo.blockWidth - 1) / formatInfo.blockWidth;
    size_t blocksY = (mipHeight + formatInfo.blockHeight - 1) / formatInfo.blockHeight;
    size_t blocksZ = (mipLayers + formatInfo.blockDepth - 1) / formatInfo.blockDepth;

    size_t bytesPerRow = alignTo(blocksX * formatInfo.bytesPerBlock, kBytesPerRowAlignment);
    size_t sliceBytes = bytesPerRow * blocksY;
    totalMemory += sliceBytes * blocksZ;
  }

  if (!is3DTexture) {
    totalMemory *= std::max<uint32_t>(1, depthOrArrayLayers);
  }

  // Multisampled textures allocate per-sample storage.
  if (dimension == wgpu::TextureDimension::e2D) {
    totalMemory *= sampleCount;
  }

  return totalMemory;
}

} // namespace rnwgpu
