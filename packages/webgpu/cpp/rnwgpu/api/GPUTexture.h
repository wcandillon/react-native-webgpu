#pragma once

#include <algorithm>
#include <memory>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUTextureView.h"
#include "GPUTextureViewDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPUTexture : public m::HybridObject {
public:
  explicit GPUTexture(wgpu::Texture instance, std::string label)
      : HybridObject("GPUTexture"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPUTextureView> createView(
      std::optional<std::shared_ptr<GPUTextureViewDescriptor>> descriptor);
  void destroy();

  uint32_t getWidth();
  uint32_t getHeight();
  uint32_t getDepthOrArrayLayers();
  uint32_t getMipLevelCount();
  uint32_t getSampleCount();
  wgpu::TextureDimension getDimension();
  wgpu::TextureFormat getFormat();
  double getUsage();

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUTexture::getBrand, this);
    registerHybridMethod("createView", &GPUTexture::createView, this);
    registerHybridMethod("destroy", &GPUTexture::destroy, this);
    registerHybridGetter("width", &GPUTexture::getWidth, this);
    registerHybridGetter("height", &GPUTexture::getHeight, this);
    registerHybridGetter("depthOrArrayLayers",
                         &GPUTexture::getDepthOrArrayLayers, this);
    registerHybridGetter("mipLevelCount", &GPUTexture::getMipLevelCount, this);
    registerHybridGetter("sampleCount", &GPUTexture::getSampleCount, this);
    registerHybridGetter("dimension", &GPUTexture::getDimension, this);
    registerHybridGetter("format", &GPUTexture::getFormat, this);
    registerHybridGetter("usage", &GPUTexture::getUsage, this);
    registerHybridGetter("label", &GPUTexture::getLabel, this);
    registerHybridSetter("label", &GPUTexture::setLabel, this);
  }

  inline const wgpu::Texture get() { return _instance; }

  size_t getMemoryPressure() override {
    struct FormatMemoryInfo {
      uint32_t blockWidth;
      uint32_t blockHeight;
      size_t bytesPerBlock;
      bool isCompressed;
    };

    auto getFormatInfo = [](wgpu::TextureFormat format) -> FormatMemoryInfo {
      using Info = FormatMemoryInfo;
      switch (format) {
      case wgpu::TextureFormat::R8Unorm:
      case wgpu::TextureFormat::R8Snorm:
      case wgpu::TextureFormat::R8Uint:
      case wgpu::TextureFormat::R8Sint:
      case wgpu::TextureFormat::Stencil8:
        return {1, 1, 1, false};
      case wgpu::TextureFormat::R16Unorm:
      case wgpu::TextureFormat::R16Snorm:
      case wgpu::TextureFormat::R16Uint:
      case wgpu::TextureFormat::R16Sint:
      case wgpu::TextureFormat::R16Float:
      case wgpu::TextureFormat::RG8Unorm:
      case wgpu::TextureFormat::RG8Snorm:
      case wgpu::TextureFormat::RG8Uint:
      case wgpu::TextureFormat::RG8Sint:
      case wgpu::TextureFormat::Depth16Unorm:
        return {1, 1, 2, false};
      case wgpu::TextureFormat::R32Float:
      case wgpu::TextureFormat::R32Uint:
      case wgpu::TextureFormat::R32Sint:
      case wgpu::TextureFormat::RG16Unorm:
      case wgpu::TextureFormat::RG16Snorm:
      case wgpu::TextureFormat::RG16Uint:
      case wgpu::TextureFormat::RG16Sint:
      case wgpu::TextureFormat::RG16Float:
      case wgpu::TextureFormat::RGBA8Unorm:
      case wgpu::TextureFormat::RGBA8UnormSrgb:
      case wgpu::TextureFormat::RGBA8Snorm:
      case wgpu::TextureFormat::RGBA8Uint:
      case wgpu::TextureFormat::RGBA8Sint:
      case wgpu::TextureFormat::BGRA8Unorm:
      case wgpu::TextureFormat::BGRA8UnormSrgb:
      case wgpu::TextureFormat::RGB10A2Unorm:
      case wgpu::TextureFormat::RGB10A2Uint:
      case wgpu::TextureFormat::RG11B10Ufloat:
      case wgpu::TextureFormat::RGB9E5Ufloat:
      case wgpu::TextureFormat::Depth24Plus:
      case wgpu::TextureFormat::Depth24PlusStencil8:
      case wgpu::TextureFormat::Depth32Float:
      case wgpu::TextureFormat::External:
        return {1, 1, 4, false};
      case wgpu::TextureFormat::RG32Float:
      case wgpu::TextureFormat::RG32Uint:
      case wgpu::TextureFormat::RG32Sint:
      case wgpu::TextureFormat::RGBA16Unorm:
      case wgpu::TextureFormat::RGBA16Snorm:
      case wgpu::TextureFormat::RGBA16Uint:
      case wgpu::TextureFormat::RGBA16Sint:
      case wgpu::TextureFormat::RGBA16Float:
      case wgpu::TextureFormat::Depth32FloatStencil8:
        return {1, 1, 8, false};
      case wgpu::TextureFormat::RGBA32Float:
      case wgpu::TextureFormat::RGBA32Uint:
      case wgpu::TextureFormat::RGBA32Sint:
        return {1, 1, 16, false};
      case wgpu::TextureFormat::BC1RGBAUnorm:
      case wgpu::TextureFormat::BC1RGBAUnormSrgb:
      case wgpu::TextureFormat::BC4RUnorm:
      case wgpu::TextureFormat::BC4RSnorm:
      case wgpu::TextureFormat::ETC2RGB8Unorm:
      case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
      case wgpu::TextureFormat::ETC2RGB8A1Unorm:
      case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
      case wgpu::TextureFormat::EACR11Unorm:
      case wgpu::TextureFormat::EACR11Snorm:
        return {4, 4, 8, true};
      case wgpu::TextureFormat::BC2RGBAUnorm:
      case wgpu::TextureFormat::BC2RGBAUnormSrgb:
      case wgpu::TextureFormat::BC3RGBAUnorm:
      case wgpu::TextureFormat::BC3RGBAUnormSrgb:
      case wgpu::TextureFormat::BC5RGUnorm:
      case wgpu::TextureFormat::BC5RGSnorm:
      case wgpu::TextureFormat::BC6HRGBUfloat:
      case wgpu::TextureFormat::BC6HRGBFloat:
      case wgpu::TextureFormat::BC7RGBAUnorm:
      case wgpu::TextureFormat::BC7RGBAUnormSrgb:
      case wgpu::TextureFormat::ETC2RGBA8Unorm:
      case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
      case wgpu::TextureFormat::EACRG11Unorm:
      case wgpu::TextureFormat::EACRG11Snorm:
        return {4, 4, 16, true};
      case wgpu::TextureFormat::ASTC4x4Unorm:
      case wgpu::TextureFormat::ASTC4x4UnormSrgb:
        return {4, 4, 16, true};
      case wgpu::TextureFormat::ASTC5x4Unorm:
      case wgpu::TextureFormat::ASTC5x4UnormSrgb:
        return {5, 4, 16, true};
      case wgpu::TextureFormat::ASTC5x5Unorm:
      case wgpu::TextureFormat::ASTC5x5UnormSrgb:
        return {5, 5, 16, true};
      case wgpu::TextureFormat::ASTC6x5Unorm:
      case wgpu::TextureFormat::ASTC6x5UnormSrgb:
        return {6, 5, 16, true};
      case wgpu::TextureFormat::ASTC6x6Unorm:
      case wgpu::TextureFormat::ASTC6x6UnormSrgb:
        return {6, 6, 16, true};
      case wgpu::TextureFormat::ASTC8x5Unorm:
      case wgpu::TextureFormat::ASTC8x5UnormSrgb:
        return {8, 5, 16, true};
      case wgpu::TextureFormat::ASTC8x6Unorm:
      case wgpu::TextureFormat::ASTC8x6UnormSrgb:
        return {8, 6, 16, true};
      case wgpu::TextureFormat::ASTC8x8Unorm:
      case wgpu::TextureFormat::ASTC8x8UnormSrgb:
        return {8, 8, 16, true};
      case wgpu::TextureFormat::ASTC10x5Unorm:
      case wgpu::TextureFormat::ASTC10x5UnormSrgb:
        return {10, 5, 16, true};
      case wgpu::TextureFormat::ASTC10x6Unorm:
      case wgpu::TextureFormat::ASTC10x6UnormSrgb:
        return {10, 6, 16, true};
      case wgpu::TextureFormat::ASTC10x8Unorm:
      case wgpu::TextureFormat::ASTC10x8UnormSrgb:
        return {10, 8, 16, true};
      case wgpu::TextureFormat::ASTC10x10Unorm:
      case wgpu::TextureFormat::ASTC10x10UnormSrgb:
        return {10, 10, 16, true};
      case wgpu::TextureFormat::ASTC12x10Unorm:
      case wgpu::TextureFormat::ASTC12x10UnormSrgb:
        return {12, 10, 16, true};
      case wgpu::TextureFormat::ASTC12x12Unorm:
      case wgpu::TextureFormat::ASTC12x12UnormSrgb:
        return {12, 12, 16, true};
      case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
      case wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm:
      case wgpu::TextureFormat::R8BG8A8Triplanar420Unorm:
      case wgpu::TextureFormat::R8BG8Biplanar422Unorm:
      case wgpu::TextureFormat::R10X6BG10X6Biplanar422Unorm:
        return {1, 1, 2, false};
      case wgpu::TextureFormat::R8BG8Biplanar444Unorm:
        return {1, 1, 3, false};
      case wgpu::TextureFormat::R10X6BG10X6Biplanar444Unorm:
        return {1, 1, 6, false};
      case wgpu::TextureFormat::Undefined:
      default:
        return {1, 1, 4, false};
      }
    };

    const uint32_t width = getWidth();
    const uint32_t height = getHeight();
    const uint32_t depthOrArrayLayers = getDepthOrArrayLayers();
    const uint32_t mipLevelCount = getMipLevelCount();
    const uint32_t sampleCount = std::max(1u, getSampleCount());
    const bool is3D = getDimension() == wgpu::TextureDimension::e3D;

    FormatMemoryInfo info = getFormatInfo(getFormat());

    size_t totalMemory = 0;
    for (uint32_t mip = 0; mip < mipLevelCount; ++mip) {
      const uint32_t mipWidth = std::max(1u, width >> mip);
      const uint32_t mipHeight = std::max(1u, height >> mip);
      const uint32_t mipDepthOrLayers =
          is3D ? std::max(1u, depthOrArrayLayers >> mip) : depthOrArrayLayers;
      const size_t layerMultiplier =
          static_cast<size_t>(mipDepthOrLayers) * sampleCount;

      if (info.isCompressed) {
        const uint32_t blocksWide =
            (mipWidth + info.blockWidth - 1) / info.blockWidth;
        const uint32_t blocksHigh =
            (mipHeight + info.blockHeight - 1) / info.blockHeight;
        totalMemory += static_cast<size_t>(blocksWide) * blocksHigh *
                       info.bytesPerBlock * layerMultiplier;
      } else {
        totalMemory += static_cast<size_t>(mipWidth) * mipHeight *
                       info.bytesPerBlock * layerMultiplier;
      }
    }

    constexpr size_t kAlignment = 4 * 1024; // keep GC pressure conservative
    if (totalMemory == 0) {
      totalMemory = kAlignment;
    } else {
      totalMemory = ((totalMemory + kAlignment - 1) / kAlignment) * kAlignment;
    }
    return totalMemory;
  }

private:
  wgpu::Texture _instance;
  std::string _label;
};

} // namespace rnwgpu
