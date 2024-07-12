#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"
#include <RNFHybridObject.h>

#include "AsyncRunner.h"
#include "MutableBuffer.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  explicit GPU(wgpu::Instance instance, std::shared_ptr<AsyncRunner> async)
      : HybridObject("GPU"), _instance(instance), _async(async) {}

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

private:
  wgpu::Instance _instance;
  std::shared_ptr<AsyncRunner> _async;
};
} // namespace rnwgpu