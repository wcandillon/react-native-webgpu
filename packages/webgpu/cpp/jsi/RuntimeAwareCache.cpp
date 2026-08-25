#include "RuntimeAwareCache.h"

namespace rnwgpu {

std::atomic<jsi::Runtime *> BaseRuntimeAwareCache::_mainRuntime{nullptr};

} // namespace rnwgpu
