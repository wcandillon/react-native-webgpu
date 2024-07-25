#pragma once

#include <string>
#include <memory>


#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"
#include "GPUTexture.h"
#include "GPUCanvasConfiguration.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCanvasContext : public m::HybridObject {
public:
  explicit GPUCanvasContext(const SurfaceData &surfaceData, std::string label)
      : HybridObject("GPUCanvasContext"),
        _instance(*surfaceData.surface),
        _clientWidth(surfaceData.clientWidth),
        _clientHeight(surfaceData.clientHeight),
        _width(surfaceData.width),
        _height(surfaceData.height),
        _label(label) {}

public:
  std::string getBrand() { return _name; }
  float getWidth() { return _width; }
  float getHeight() { return _height; }
  float getClientWidth() { return _clientWidth; }
  float getClientHeight() { return _clientHeight; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCanvasContext::getBrand, this);
    registerHybridGetter("width", &GPUCanvasContext::getWidth, this);
    registerHybridGetter("height", &GPUCanvasContext::getHeight, this);
    registerHybridGetter("clientWidth", &GPUCanvasContext::getClientWidth, this);
    registerHybridGetter("clientHeight", &GPUCanvasContext::getClientHeight, this);
    registerHybridMethod("configure", &GPUCanvasContext::configure, this);
    registerHybridMethod("unconfigure", &GPUCanvasContext::unconfigure, this);
    registerHybridMethod("getCurrentTexture", &GPUCanvasContext::getCurrentTexture, this);
    registerHybridMethod("present", &GPUCanvasContext::present, this);
  }

  inline const wgpu::Surface get() { return _instance; }
  void configure(std::shared_ptr<GPUCanvasConfiguration> configuration);
  void unconfigure();
  std::shared_ptr<GPUTexture> getCurrentTexture();
  void present();

private:
  wgpu::Surface _instance;
  float _clientWidth;
  float _clientHeight;
  float _width;
  float _height;
  std::string _label;
};

} // namespace rnwgpu
