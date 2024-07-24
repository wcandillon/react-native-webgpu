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
  explicit GPUCanvasContext(wgpu::Surface instance, int width, int height, std::string label)
      : HybridObject("GPUCanvasContext"),
        _instance(instance),
        _width(width),
        _height(height),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCanvasContext::getBrand, this);
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
  int _width;
  int _height;
  std::string _label;
};

} // namespace rnwgpu
