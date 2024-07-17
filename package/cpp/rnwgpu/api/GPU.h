#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "Unions.h"
#include <RNFHybridObject.h>

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  GPU() : HybridObject("GPU") {
    wgpu::InstanceDescriptor instanceDesc;
    instanceDesc.features.timedWaitAnyEnable = true;
    instanceDesc.features.timedWaitAnyMaxCount = 64;
    _instance = wgpu::CreateInstance(&instanceDesc);
    auto instance = &_instance;
    _async = std::make_shared<AsyncRunner>(instance);
  }

public:
  std::string getBrand() { return _name; }

  std::future<std::shared_ptr<GPUAdapter>>
  requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options);
  wgpu::TextureFormat getPreferredCanvasFormat();

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPU::getBrand, this);
    registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
    registerHybridMethod("getPreferredCanvasFormat",
                         &GPU::getPreferredCanvasFormat, this);
  }

  inline const wgpu::Instance get() { return _instance; }

private:
  wgpu::Instance _instance;
  std::shared_ptr<AsyncRunner> _async;
};
} // namespace rnwgpu