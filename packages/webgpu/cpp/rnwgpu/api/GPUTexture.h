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
    // Calculate approximate memory usage based on texture properties
    uint32_t width = getWidth();
    uint32_t height = getHeight();
    uint32_t depthOrArrayLayers = getDepthOrArrayLayers();
    uint32_t mipLevelCount = getMipLevelCount();
    uint32_t sampleCount = getSampleCount();

    // Estimate bytes per pixel based on format
    // This is a simplified estimate - actual values depend on the specific
    // format
    size_t bytesPerPixel = 4; // Default to RGBA8 format
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
      bytesPerPixel = 4; // Safe default
      break;
    }

    // Calculate total memory for all mip levels
    size_t totalMemory = 0;
    for (uint32_t mip = 0; mip < mipLevelCount; ++mip) {
      uint32_t mipWidth = std::max(1u, width >> mip);
      uint32_t mipHeight = std::max(1u, height >> mip);
      totalMemory += static_cast<size_t>(mipWidth) * mipHeight *
                     depthOrArrayLayers * bytesPerPixel * sampleCount;
    }

    return totalMemory;
  }

private:
  wgpu::Texture _instance;
  std::string _label;
};

} // namespace rnwgpu
