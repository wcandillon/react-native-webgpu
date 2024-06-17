#include "RNWebGPUManager.h"

namespace rnwgpu {
RNWebGPUManager::RNWebGPUManager(
    jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker) {

  // Register main runtime
  // BaseRuntimeAwareCache::setMainJsRuntime(_jsRuntime);

  // Install bindings
  // installBindings();
}

RNWebGPUManager::~RNWebGPUManager() {
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}
} // namespace rnwgpu
