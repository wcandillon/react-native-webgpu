#include "RNWebGPUManager.h"

#include "GPU.h"
// Enums
#include "GPUBufferUsage.h"
#include "GPUColorWrite.h"
#include "GPUMapMode.h"
#include "GPUShaderStage.h"
#include "GPUTextureUsage.h"

#include <memory>
#include <utility>

#include "dawn/dawn_proc.h"

namespace dawn {
    namespace native {
        // Forward declaration of the function
        const DawnProcTable& GetProcsAutogen();
    }
}

namespace rnwgpu {
RNWebGPUManager::RNWebGPUManager(
    jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker) {

  auto table = dawn::native::GetProcsAutogen();
  dawnProcSetProcs(&table);

  wgpu::InstanceDescriptor instanceDesc;
  instanceDesc.features.timedWaitAnyEnable = true;
  instanceDesc.features.timedWaitAnyMaxCount = 64;
  auto gpu = std::make_shared<GPU>(
      std::make_shared<wgpu::Instance>(wgpu::CreateInstance(&instanceDesc)));

  auto bufferUsage = std::make_shared<GPUBufferUsage>();
  _jsRuntime->global().setProperty(
      *_jsRuntime, "GPUBufferUsage",
      jsi::Object::createFromHostObject(*_jsRuntime, std::move(bufferUsage)));

  auto colorWrite = std::make_shared<GPUColorWrite>();
  _jsRuntime->global().setProperty(
      *_jsRuntime, "GPUColorWrite",
      jsi::Object::createFromHostObject(*_jsRuntime, std::move(colorWrite)));

  auto mapMode = std::make_shared<GPUMapMode>();
  _jsRuntime->global().setProperty(
      *_jsRuntime, "GPUMapMode",
      jsi::Object::createFromHostObject(*_jsRuntime, std::move(mapMode)));

  auto shaderStage = std::make_shared<GPUShaderStage>();
  _jsRuntime->global().setProperty(
      *_jsRuntime, "GPUShaderStage",
      jsi::Object::createFromHostObject(*_jsRuntime, std::move(shaderStage)));

  auto textureUsage = std::make_shared<GPUTextureUsage>();
  _jsRuntime->global().setProperty(
      *_jsRuntime, "GPUTextureUsage",
      jsi::Object::createFromHostObject(*_jsRuntime, std::move(textureUsage)));

  _jsRuntime->global().setProperty(
      *_jsRuntime, "gpu",
      jsi::Object::createFromHostObject(*_jsRuntime, std::move(gpu)));
}

RNWebGPUManager::~RNWebGPUManager() {
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}
} // namespace rnwgpu
