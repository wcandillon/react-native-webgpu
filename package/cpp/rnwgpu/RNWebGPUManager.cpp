#include "RNWebGPUManager.h"

#include "GPU.h"

#include <memory>
#include <utility>

namespace rnwgpu {
RNWebGPUManager::RNWebGPUManager(
    jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker) {

  wgpu::InstanceDescriptor instanceDesc;
  instanceDesc.features.timedWaitAnyEnable = true;
  instanceDesc.features.timedWaitAnyMaxCount = 64;
  auto gpu = std::make_shared<GPU>(std::make_shared<wgpu::Instance>(wgpu::CreateInstance(&instanceDesc)));

  _jsRuntime->global().setProperty(
      *_jsRuntime, "gpu",
      jsi::Object::createFromHostObject(*_jsRuntime, std::move(gpu)));
}

RNWebGPUManager::~RNWebGPUManager() {
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}
} // namespace rnwgpu
