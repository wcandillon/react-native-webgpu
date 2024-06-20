#include "GPU.h"

namespace rnwgpu {

void GPU::loadHybridMethods() {
  registerHybridGetter("__brand", &GPU::getBrand, this);
  registerHybridGetter("gpu", &GPU::getGPU, this);
}

} // namespace rnwgpu
