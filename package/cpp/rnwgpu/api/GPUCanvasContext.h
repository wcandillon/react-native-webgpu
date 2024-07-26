#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "webgpu/webgpu_cpp.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "GPUCanvasConfiguration.h"
#include "GPUTexture.h"
#include "SurfaceRegistry.h"
#include "SurfaceRegistry.h"

namespace rnwgpu {

namespace m = margelo;

class Canvas : public m::HybridObject {
public:
  explicit Canvas(const float width, const float height)
      : HybridObject("Canvas"), _width(width), _height(height) {}

  float getWidth() { return _width; }
  float getHeight() { return _height; }
  float getClientWidth() { return _width; }
  float getClientHeight() { return _height; }

  void loadHybridMethods() override {
    registerHybridGetter("width", &Canvas::getWidth, this);
    registerHybridGetter("height", &Canvas::getHeight, this);
    registerHybridGetter("clientWidth", &Canvas::getClientWidth, this);
    registerHybridGetter("clientHeight", &Canvas::getClientHeight, this);
  }

private:
  const float _width;
  const float _height;
};

class GPUCanvasContext : public m::HybridObject {
public:
  explicit GPUCanvasContext(const SurfaceData &surfaceData)
      : HybridObject("GPUCanvasContext"), _instance(*surfaceData.surface),
        _canvas(std::make_shared<rnwgpu::Canvas>(surfaceData.width,
                                                 surfaceData.height)) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<Canvas> getCanvas() { return _canvas; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCanvasContext::getBrand, this);
    registerHybridGetter("canvas", &GPUCanvasContext::getCanvas, this);
    registerHybridMethod("configure", &GPUCanvasContext::configure, this);
    registerHybridMethod("unconfigure", &GPUCanvasContext::unconfigure, this);
    registerHybridMethod("getCurrentTexture",
                         &GPUCanvasContext::getCurrentTexture, this);
    registerHybridMethod("present", &GPUCanvasContext::present, this);
  }

  inline const wgpu::Surface get() { return _instance; }
  void configure(std::shared_ptr<GPUCanvasConfiguration> configuration);
  void unconfigure();
  std::shared_ptr<GPUTexture> getCurrentTexture();
  void present();

private:
  wgpu::Surface _instance;
  std::shared_ptr<Canvas> _canvas;
};

} // namespace rnwgpu
