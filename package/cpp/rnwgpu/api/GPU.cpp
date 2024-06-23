#include "GPU.h"
#include "GPURequestAdapterOptions.h"

#include <memory>

namespace rnwgpu {

void GPU::loadHybridMethods() {
  registerHybridGetter("__brand", &GPU::getBrand, this);
  registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
}

std::future<std::shared_ptr<GPUAdapter>>
GPU::requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options) {
  // Create a shared_ptr to GPUAdapter
  return std::async(std::launch::async,
                    [=]() { return std::make_shared<GPUAdapter>(); });
}

} // namespace rnwgpu
