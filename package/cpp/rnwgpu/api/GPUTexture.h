#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

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

  std::shared_ptr<GPUTextureView>
  createView(std::shared_ptr<GPUTextureViewDescriptor> descriptor);
  void destroy();

  uint32_t getWidth();
  uint32_t getHeight();
  uint32_t getDepthOrArrayLayers();
  uint32_t getMipLevelCount();
  uint32_t getSampleCount();
  wgpu::TextureDimension getDimension();
  wgpu::TextureFormat getFormat();
  uint32_t getUsage();

  std::string getLabel() { return _label; }

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
  }

  inline const wgpu::Texture get() { return _instance; }

private:
  wgpu::Texture _instance;
  std::string _label;
};

} // namespace rnwgpu