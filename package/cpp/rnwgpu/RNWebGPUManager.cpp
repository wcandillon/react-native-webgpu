#include "RNWebGPUManager.h"

#include "JsiNavigator.h"

#include <memory>
#include <utility>

namespace rnwgpu {
RNWebGPUManager::RNWebGPUManager(
    jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker) {

  auto navigator = std::make_shared<Navigator>();

  _jsRuntime->global().setProperty(
      *_jsRuntime, "navigator",
      jsi::Object::createFromHostObject(*_jsRuntime, std::move(navigator)));
}

RNWebGPUManager::~RNWebGPUManager() {
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}
} // namespace rnwgpu
