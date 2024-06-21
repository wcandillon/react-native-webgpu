#include "GPUAdapter.h"

namespace rnwgpu {

void GPUAdapter::loadHybridMethods() {
  registerHybridGetter("__brand", &GPUAdapter::getBrand, this);
}

} // namespace rnwgpu
