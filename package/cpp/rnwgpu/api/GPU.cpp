#include "GPU.h"

namespace rnwgpu {

void GPU::loadHybridMethods() {
  registerHybridGetter("__brand", &GPU::getBrand, this);
  registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
}

std::shared_ptr<GPUAdapter> GPU::requestAdapter() {
  // Create a shared_ptr to GPUAdapter
  return std::make_shared<GPUAdapter>();
}

} // namespace rnwgpu
