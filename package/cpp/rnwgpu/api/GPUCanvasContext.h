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
  explicit GPUCanvasContext(wgpu::Surface instance)
      : HybridObject("GPUCanvasContext"), _instance(instance) {}

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
  wgpu::Device _device;
};

} // namespace rnwgpu
