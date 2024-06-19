#include "RNWebGPUManager.h"

#include <memory>

namespace rnwgpu {
RNWebGPUManager::RNWebGPUManager(
    jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker) {

  // Register main runtime
  // BaseRuntimeAwareCache::setMainJsRuntime(_jsRuntime);

  // auto skiaApi = std::make_shared<JsiSkApi>(*_jsRuntime, _platformContext);
  // _jsRuntime->global().setProperty(
  //     *_jsRuntime, "SkiaApi",
  //     jsi::Object::createFromHostObject(*_jsRuntime, std::move(skiaApi)));
}

RNWebGPUManager::~RNWebGPUManager() {
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}
} // namespace rnwgpu
