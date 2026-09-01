#include "RuntimeAwareCache.h"

namespace rnwgpu {

std::atomic<jsi::Runtime *> BaseRuntimeAwareCache::_mainRuntime{nullptr};
std::atomic<uint64_t> BaseRuntimeAwareCache::_mainRuntimeGeneration{0};

} // namespace rnwgpu
