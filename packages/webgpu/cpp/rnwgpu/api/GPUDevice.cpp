#include "GPUDevice.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Convertors.h"
#include "RNFJSIConverter.h"

#include "GPUFeatures.h"

namespace rnwgpu {

void GPUDevice::initializeCallbacks() {
  std::weak_ptr<GPUDevice> weakSelf = shared<GPUDevice>();
  // _instance.SetDeviceLostCallback(
  //     wgpu::CallbackMode::AllowProcessEvents,
  //     [weakSelf](const wgpu::Device & /*device*/, wgpu::DeviceLostReason reason,
  //                wgpu::StringView message) {
  //       if (auto self = weakSelf.lock()) {
  //         std::string msg =
  //             message.length ? std::string(message.data, message.length) : "";
  //         self->notifyDeviceLost(reason, std::move(msg));
  //       }
  //     });
}

void GPUDevice::notifyDeviceLost(wgpu::DeviceLostReason reason,
                                 std::string message) {
  if (_lostSettled) {
    return;
  }

  _lostSettled = true;
  _lostInfo = std::make_shared<GPUDeviceLostInfo>(reason, std::move(message));

  if (_lostResolve.has_value()) {
    auto resolve = std::move(*_lostResolve);
    _lostResolve.reset();
    resolve([info = _lostInfo](jsi::Runtime& runtime) mutable {
      return margelo::JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(runtime, info);
    });
  }

}

std::shared_ptr<GPUBuffer>
GPUDevice::createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor) {
  wgpu::BufferDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error(
        "GPUDevice::createBuffer(): Error with GPUBufferDescriptor");
  }
  auto result = _instance.CreateBuffer(&desc);
  return std::make_shared<GPUBuffer>(result, _async,
                                     descriptor->label.value_or(""));
}

std::shared_ptr<GPUSupportedLimits> GPUDevice::getLimits() {
  wgpu::Limits limits{};
  if (!_instance.GetLimits(&limits)) {
    throw std::runtime_error("failed to get device limits");
  }
  return std::make_shared<GPUSupportedLimits>(limits);
}

std::shared_ptr<GPUQueue> GPUDevice::getQueue() {
  auto result = _instance.GetQueue();
  return std::make_shared<GPUQueue>(result, _async, _label);
}

std::shared_ptr<GPUCommandEncoder> GPUDevice::createCommandEncoder(
    std::optional<std::shared_ptr<GPUCommandEncoderDescriptor>> descriptor) {
  wgpu::CommandEncoderDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPUCommandEncoderDescriptor");
  }
  auto result = _instance.CreateCommandEncoder(&desc);
  return std::make_shared<GPUCommandEncoder>(
      result,
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "");
}

void GPUDevice::destroy() {
  _instance.Destroy();
  notifyDeviceLost(wgpu::DeviceLostReason::Destroyed, "device was destroyed");
}

std::shared_ptr<GPUTexture>
GPUDevice::createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor) {
  wgpu::TextureDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPUTextureDescriptor");
  }
  auto texture = _instance.CreateTexture(&desc);
  return std::make_shared<GPUTexture>(texture, descriptor->label.value_or(""));
}

std::shared_ptr<GPUShaderModule> GPUDevice::createShaderModule(
    std::shared_ptr<GPUShaderModuleDescriptor> descriptor) {
  wgpu::ShaderSourceWGSL wgsl_desc{};
  wgpu::ShaderModuleDescriptor sm_desc{};
  Convertor conv;
  if (!conv(wgsl_desc.code, descriptor->code) ||
      !conv(sm_desc.label, descriptor->label)) {
    return {};
  }
  sm_desc.nextInChain = &wgsl_desc;
  if (descriptor->code.find('\0') != std::string::npos) {
    auto mod = _instance.CreateErrorShaderModule(
        &sm_desc, "The WGSL shader contains an illegal character '\\0'");
    return std::make_shared<GPUShaderModule>(mod, _async, sm_desc.label.data);
  }
  auto module = _instance.CreateShaderModule(&sm_desc);
  return std::make_shared<GPUShaderModule>(module, _async,
                                           descriptor->label.value_or(""));
}

std::shared_ptr<GPURenderPipeline> GPUDevice::createRenderPipeline(
    std::shared_ptr<GPURenderPipelineDescriptor> descriptor) {
  wgpu::RenderPipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPURenderPipelineDescriptor");
  }
  // assert(desc.fragment != nullptr && "Fragment state must not be null");
  auto renderPipeline = _instance.CreateRenderPipeline(&desc);
  return std::make_shared<GPURenderPipeline>(renderPipeline,
                                             descriptor->label.value_or(""));
}

std::shared_ptr<GPUBindGroup>
GPUDevice::createBindGroup(std::shared_ptr<GPUBindGroupDescriptor> descriptor) {
  Convertor conv;
  wgpu::BindGroupDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.layout, descriptor->layout) ||
      !conv(desc.entries, desc.entryCount, descriptor->entries)) {
    throw std::runtime_error(
        "GPUBindGroup::createBindGroup(): Error with GPUBindGroupDescriptor");
  }
  auto bindGroup = _instance.CreateBindGroup(&desc);
  return std::make_shared<GPUBindGroup>(bindGroup,
                                        descriptor->label.value_or(""));
}

std::shared_ptr<GPUSampler> GPUDevice::createSampler(
    std::optional<std::shared_ptr<GPUSamplerDescriptor>> descriptor) {
  wgpu::SamplerDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createSampler(): Error with "
                             "GPUSamplerDescriptor");
  }
  auto sampler = _instance.CreateSampler(&desc);
  return std::make_shared<GPUSampler>(
      sampler,
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "");
}

std::shared_ptr<GPUComputePipeline> GPUDevice::createComputePipeline(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  wgpu::ComputePipelineDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createComputePipeline(): Error with "
                             "GPUComputePipelineDescriptor");
  }
  auto computePipeline = _instance.CreateComputePipeline(&desc);
  return std::make_shared<GPUComputePipeline>(computePipeline,
                                              descriptor->label.value_or(""));
}

std::shared_ptr<GPUQuerySet>
GPUDevice::createQuerySet(std::shared_ptr<GPUQuerySetDescriptor> descriptor) {
  wgpu::QuerySetDescriptor desc;
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createQuerySet(): Error with "
                             "GPUQuerySetDescriptor");
  }
  auto querySet = _instance.CreateQuerySet(&desc);
  return std::make_shared<GPUQuerySet>(querySet,
                                       descriptor->label.value_or(""));
}

std::shared_ptr<GPURenderBundleEncoder> GPUDevice::createRenderBundleEncoder(
    std::shared_ptr<GPURenderBundleEncoderDescriptor> descriptor) {
  Convertor conv;

  wgpu::RenderBundleEncoderDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.colorFormats, desc.colorFormatCount,
            descriptor->colorFormats) ||
      !conv(desc.depthStencilFormat, descriptor->depthStencilFormat) ||
      !conv(desc.sampleCount, descriptor->sampleCount) ||
      !conv(desc.depthReadOnly, descriptor->depthReadOnly) ||
      !conv(desc.stencilReadOnly, descriptor->stencilReadOnly)) {
    return {};
  }
  return std::make_shared<GPURenderBundleEncoder>(
      _instance.CreateRenderBundleEncoder(&desc),
      descriptor->label.value_or(""));
}

std::shared_ptr<GPUBindGroupLayout> GPUDevice::createBindGroupLayout(
    std::shared_ptr<GPUBindGroupLayoutDescriptor> descriptor) {
  Convertor conv;

  wgpu::BindGroupLayoutDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.entries, desc.entryCount, descriptor->entries)) {
    return {};
  }
  return std::make_shared<GPUBindGroupLayout>(
      _instance.CreateBindGroupLayout(&desc), descriptor->label.value_or(""));
}

std::shared_ptr<GPUPipelineLayout> GPUDevice::createPipelineLayout(
    std::shared_ptr<GPUPipelineLayoutDescriptor> descriptor) {
  Convertor conv;

  wgpu::PipelineLayoutDescriptor desc{};
  if (!conv(desc.label, descriptor->label) ||
      !conv(desc.bindGroupLayouts, desc.bindGroupLayoutCount,
            descriptor->bindGroupLayouts)) {
    return {};
  }
  return std::make_shared<GPUPipelineLayout>(
      _instance.CreatePipelineLayout(&desc), descriptor->label.value_or(""));
}

std::shared_ptr<GPUExternalTexture> GPUDevice::importExternalTexture(
    std::shared_ptr<GPUExternalTextureDescriptor> descriptor) {
  throw std::runtime_error(
      "GPUDevice::importExternalTexture(): Not implemented");
}

async::AsyncTaskHandle GPUDevice::createComputePipelineAsync(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  wgpu::ComputePipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("GPUDevice::createComputePipeline(): Error with "
                             "GPUComputePipelineDescriptor");
  }

  auto label = std::string(
      descriptor->label.has_value() ? descriptor->label.value() : "");
  auto pipelineHolder = std::make_shared<GPUComputePipeline>(nullptr, label);

  return _async->postTask([
    device = _instance,
    desc,
    descriptor,
    pipelineHolder
  ](const async::AsyncTaskHandle::ResolveFunction& resolve,
    const async::AsyncTaskHandle::RejectFunction& reject) {
    (void)descriptor;
    device.CreateComputePipelineAsync(
        &desc, wgpu::CallbackMode::AllowProcessEvents,
        [pipelineHolder, resolve, reject](wgpu::CreatePipelineAsyncStatus status,
                                          wgpu::ComputePipeline pipeline,
                                          const char *msg) mutable {
          if (status == wgpu::CreatePipelineAsyncStatus::Success && pipeline) {
            pipelineHolder->_instance = pipeline;
            resolve([pipelineHolder](jsi::Runtime& runtime) mutable {
              return margelo::JSIConverter<std::shared_ptr<GPUComputePipeline>>::toJSI(runtime, pipelineHolder);
            });
          } else {
            std::string error = msg ? std::string(msg) : "Failed to create compute pipeline";
            reject(std::move(error));
          }
        });
  });
}

async::AsyncTaskHandle GPUDevice::createRenderPipelineAsync(
    std::shared_ptr<GPURenderPipelineDescriptor> descriptor) {
  wgpu::RenderPipelineDescriptor desc{};
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error(
        "GPUDevice::createRenderPipelineAsync(): Error with "
        "GPURenderPipelineDescriptor");
  }

  auto label = std::string(
      descriptor->label.has_value() ? descriptor->label.value() : "");
  auto pipelineHolder = std::make_shared<GPURenderPipeline>(nullptr, label);

  return _async->postTask([
    device = _instance,
    desc,
    descriptor,
    pipelineHolder
  ](const async::AsyncTaskHandle::ResolveFunction& resolve,
    const async::AsyncTaskHandle::RejectFunction& reject) {
    (void)descriptor;
    device.CreateRenderPipelineAsync(
        &desc, wgpu::CallbackMode::AllowProcessEvents,
        [pipelineHolder, resolve, reject](wgpu::CreatePipelineAsyncStatus status,
                                          wgpu::RenderPipeline pipeline,
                                          const char *msg) mutable {
          if (status == wgpu::CreatePipelineAsyncStatus::Success && pipeline) {
            pipelineHolder->_instance = pipeline;
            resolve([pipelineHolder](jsi::Runtime& runtime) mutable {
              return margelo::JSIConverter<std::shared_ptr<GPURenderPipeline>>::toJSI(runtime, pipelineHolder);
            });
          } else {
            std::string error = msg ? std::string(msg) : "Failed to create render pipeline";
            reject(std::move(error));
          }
        });
  });
}

void GPUDevice::pushErrorScope(wgpu::ErrorFilter filter) {
  _instance.PushErrorScope(filter);
}

async::AsyncTaskHandle GPUDevice::popErrorScope() {
  auto device = _instance;

  return _async->postTask([
    device
  ](const async::AsyncTaskHandle::ResolveFunction& resolve,
    const async::AsyncTaskHandle::RejectFunction& reject) {
    device.PopErrorScope(
        wgpu::CallbackMode::AllowProcessEvents,
        [resolve, reject](wgpu::PopErrorScopeStatus status, wgpu::ErrorType type,
                          wgpu::StringView message) {
          if (status == wgpu::PopErrorScopeStatus::Error ||
              status == wgpu::PopErrorScopeStatus::CallbackCancelled) {
            reject("PopErrorScope failed");
            return;
          }

          std::variant<std::nullptr_t, std::shared_ptr<GPUError>> result = nullptr;

          switch (type) {
          case wgpu::ErrorType::NoError:
            break;
          case wgpu::ErrorType::OutOfMemory:
          case wgpu::ErrorType::Validation:
          case wgpu::ErrorType::Internal:
          case wgpu::ErrorType::Unknown: {
            std::string messageString =
                message.length ? std::string(message.data, message.length) : "";
            result = std::make_shared<GPUError>(type, messageString);
            break;
          }
          default:
            reject("Unhandled GPU error type");
            return;
          }

          resolve([result = std::move(result)](jsi::Runtime& runtime) mutable {
            return margelo::JSIConverter<decltype(result)>::toJSI(runtime, result);
          });
        });
  });
}

std::unordered_set<std::string> GPUDevice::getFeatures() {
  wgpu::SupportedFeatures supportedFeatures;
  _instance.GetFeatures(&supportedFeatures);
  std::unordered_set<std::string> result;
  for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
    auto feature = supportedFeatures.features[i];
    std::string name;
    convertEnumToJSUnion(feature, &name);
    result.insert(name);
  }
  return result;
}

async::AsyncTaskHandle GPUDevice::getLost() {
  if (_lostHandle.has_value()) {
    return *_lostHandle;
  }

  if (_lostSettled && _lostInfo) {
    return _async->postTask([
      info = _lostInfo
    ](const async::AsyncTaskHandle::ResolveFunction& resolve,
      const async::AsyncTaskHandle::RejectFunction& /*reject*/) {
      resolve([info](jsi::Runtime& runtime) mutable {
        return margelo::JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(runtime, info);
      });
    });
  }

  auto handle = _async->postTask([
    this
  ](const async::AsyncTaskHandle::ResolveFunction& resolve,
    const async::AsyncTaskHandle::RejectFunction& /*reject*/) {
    if (_lostSettled && _lostInfo) {
      resolve([info = _lostInfo](jsi::Runtime& runtime) mutable {
        return margelo::JSIConverter<std::shared_ptr<GPUDeviceLostInfo>>::toJSI(runtime, info);
      });
      return;
    }

    _lostResolve = resolve;
  });

  _lostHandle = handle;
  return handle;
}
} // namespace rnwgpu
