#include "JsiNavigator.h"

namespace rnwgpu {

void Navigator::loadHybridMethods() {
  registerHybridGetter("gpu", &Navigator::getGPU, this);
}

} // namespace rnwgpu
