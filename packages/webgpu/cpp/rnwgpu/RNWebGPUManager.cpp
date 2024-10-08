#include "RNWebGPUManager.h"

#include "CallInvokerDispatcher.h"
#include "Dispatcher.h"
#include "GPU.h"
#include "RNWebGPU.h"

// Enums
#include "GPUBufferUsage.h"
#include "GPUColorWrite.h"
#include "GPUMapMode.h"
#include "GPUShaderStage.h"
#include "GPUTextureUsage.h"

#include <memory>
#include <utility>

namespace rnwgpu {

RNWebGPUManager::RNWebGPUManager(
    jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
    std::shared_ptr<PlatformContext> platformContext)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker),
      _platformContext(platformContext) {

  // Installs the global Dispatcher mechanism into this Runtime.
  // This allows creating Promises and calling back to JS.
  auto dispatcher =
      std::make_shared<margelo::CallInvokerDispatcher>(_jsCallInvoker);
  margelo::Dispatcher::installRuntimeGlobalDispatcher(*_jsRuntime, dispatcher);

  auto gpu = std::make_shared<GPU>();
  auto rnWebGPU = std::make_shared<RNWebGPU>(gpu, _platformContext);
  _jsRuntime->global().setProperty(
      *_jsRuntime, "RNWebGPU",
      jsi::Object::createFromHostObject(*_jsRuntime, rnWebGPU));

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
}

RNWebGPUManager::~RNWebGPUManager() {
  _jsRuntime = nullptr;
  _jsCallInvoker = nullptr;
}

} // namespace rnwgpu
