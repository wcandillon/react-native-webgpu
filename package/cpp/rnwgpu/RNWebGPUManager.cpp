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

namespace rnwgpu {

std::shared_ptr<wgpu::Surface> RNWebGPUManager::surface;

RNWebGPUManager::RNWebGPUManager(
    jsi::Runtime *jsRuntime,
    std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker)
    : _jsRuntime(jsRuntime), _jsCallInvoker(jsCallInvoker) {

  auto gpu = std::make_shared<GPU>();
  this->gpu = gpu;

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
