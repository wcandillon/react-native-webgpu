#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "GPU.h"
#include "GPURequestAdapterOptions.h"


namespace rnwgpu {

GPU::GPU(): HybridObject("GPU") {
  wgpu::InstanceDescriptor instanceDesc;
  instanceDesc.features.timedWaitAnyEnable = true;
  instanceDesc.features.timedWaitAnyMaxCount = 64;
  _instance = wgpu::CreateInstance(&instanceDesc);
}

void GPU::loadHybridMethods() {
  registerHybridGetter("__brand", &GPU::getBrand, this);
  registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
}

std::future<std::shared_ptr<GPUAdapter>>
GPU::requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options) {
  wgpu::RequestAdapterOptions defaultOptions;
  // Create a shared_ptr to GPUAdapter
  return std::async(std::launch::async,
                    [=]() { return std::make_shared<GPUAdapter>(); });
}

} // namespace rnwgpu
