#include "GPUDevice.h"

#include <vector>

#include "Convertors.h"

#include "GPUFeatures.h"

namespace rnwgpu {

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
  wgpu::SupportedLimits limits{};
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
  auto lostInfo = std::make_shared<GPUDeviceLostInfo>(wgpu::DeviceLostReason::Destroyed, "device was destroyed");
  m_lostPromise->set_value(lostInfo);
}

std::shared_ptr<GPUTexture>
GPUDevice::createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor) {
  wgpu::TextureDescriptor desc;
  // TODO: implement
  Convertor conv;
  if (!conv(desc, descriptor)) {
    throw std::runtime_error("Error with GPUTextureDescriptor");
  }
  auto texture = _instance.CreateTexture(&desc);
  return std::make_shared<GPUTexture>(texture, descriptor->label.value_or(""));
}

std::shared_ptr<GPUShaderModule> GPUDevice::createShaderModule(
    std::shared_ptr<GPUShaderModuleDescriptor> descriptor) {
  wgpu::ShaderModuleWGSLDescriptor wgsl_desc{};
  wgpu::ShaderModuleDescriptor sm_desc{};
  wgsl_desc.code = descriptor->code.c_str();
  sm_desc.label = descriptor->label.value_or("").c_str();
  sm_desc.nextInChain = &wgsl_desc;
  if (descriptor->code.find('\0') != std::string::npos) {
    return std::make_shared<GPUShaderModule>(
        _instance.CreateErrorShaderModule(
            &sm_desc, "The WGSL shader contains an illegal character '\\0'"),
        _async, descriptor->label.value_or(""));
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
  assert(desc.fragment != nullptr && "Fragment state must not be null");
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

std::future<std::shared_ptr<GPUComputePipeline>>
GPUDevice::createComputePipelineAsync(
    std::shared_ptr<GPUComputePipelineDescriptor> descriptor) {
  return _async->runAsync([=](wgpu::Instance *instance) {
    wgpu::ComputePipelineDescriptor desc{};
    Convertor conv;
    if (!conv(desc, descriptor)) {
      throw std::runtime_error("GPUDevice::createComputePipeline(): Error with "
                               "GPUComputePipelineDescriptor");
    }
    wgpu::ComputePipeline computePipeline = nullptr;
    auto label = std::string(
        descriptor->label.has_value() ? descriptor->label.value() : "");
    auto result = std::make_shared<GPUComputePipeline>(computePipeline, label);
    auto future = _instance.CreateComputePipelineAsync(
        &desc, wgpu::CallbackMode::WaitAnyOnly,
        [&result](wgpu::CreatePipelineAsyncStatus status,
                  wgpu::ComputePipeline pipeline, char const *msg) {
          switch (status) {
          case wgpu::CreatePipelineAsyncStatus::Success:
            result->_instance = pipeline;
            break;
          default:
            throw std::runtime_error(msg);
            break;
          }
        });
    instance->WaitAny(future, UINT64_MAX);
    return result;
  });
}

std::future<std::shared_ptr<GPURenderPipeline>>
GPUDevice::createRenderPipelineAsync(
    std::shared_ptr<GPURenderPipelineDescriptor> descriptor) {
  return _async->runAsync([=](wgpu::Instance *instance) {
    wgpu::RenderPipelineDescriptor desc{};
    Convertor conv;
    if (!conv(desc, descriptor)) {
      throw std::runtime_error(
          "GPUDevice::createRenderPipelineAsync(): Error with "
          "GPURenderPipelineDescriptor");
    }
    wgpu::RenderPipeline renderPipeline = nullptr;
    auto label = std::string(
        descriptor->label.has_value() ? descriptor->label.value() : "");
    auto result = std::make_shared<GPURenderPipeline>(renderPipeline, label);
    auto future = _instance.CreateRenderPipelineAsync(
        &desc, wgpu::CallbackMode::WaitAnyOnly,
        [&result](wgpu::CreatePipelineAsyncStatus status,
                  wgpu::RenderPipeline pipeline, char const *msg) {
          switch (status) {
          case wgpu::CreatePipelineAsyncStatus::Success:
            result->_instance = pipeline;
            break;
          default:
            throw std::runtime_error(msg);
            break;
          }
        });
    instance->WaitAny(future, UINT64_MAX);
    return result;
  });
}

void GPUDevice::pushErrorScope(wgpu::ErrorFilter filter) {
  _instance.PushErrorScope(filter);
}

std::future<std::variant<std::nullptr_t, std::shared_ptr<GPUError>>>
GPUDevice::popErrorScope() {
  return _async->runAsync([=](wgpu::Instance *instance) {
    std::variant<std::nullptr_t, std::shared_ptr<GPUError>> result = nullptr;
    auto future = _instance.PopErrorScope(
        wgpu::CallbackMode::WaitAnyOnly,
        [&result](wgpu::PopErrorScopeStatus, wgpu::ErrorType type,
                  char const *message) {
          switch (type) {
          case wgpu::ErrorType::NoError:
            break;
          case wgpu::ErrorType::OutOfMemory: {
            result = std::make_shared<GPUError>(wgpu::ErrorType::OutOfMemory,
                                                message);
            break;
          }
          case wgpu::ErrorType::Validation: {
            result = std::make_shared<GPUError>(wgpu::ErrorType::Validation,
                                                message);
            break;
          }
          case wgpu::ErrorType::Internal: {
            result =
                std::make_shared<GPUError>(wgpu::ErrorType::Internal, message);
            break;
          }
          case wgpu::ErrorType::Unknown:
          case wgpu::ErrorType::DeviceLost:
            result = std::make_shared<GPUError>(wgpu::ErrorType::DeviceLost,
                                                message);
            break;
          default:
            throw std::runtime_error(
                "unhandled error type (" +
                std::to_string(
                    static_cast<std::underlying_type<wgpu::ErrorType>::type>(
                        type)) +
                ")");
            break;
          }
        });
    instance->WaitAny(future, UINT64_MAX);
    return result;
  });
}

std::unordered_set<std::string> GPUDevice::getFeatures() {
  size_t count = _instance.EnumerateFeatures(nullptr);
  std::vector<wgpu::FeatureName> features(count);
  if (count > 0) {
    _instance.EnumerateFeatures(features.data());
  }
  std::unordered_set<std::string> result;
  for (auto feature : features) {
    std::string name;
    convertEnumToJSUnion(feature, &name);
    result.insert(name);
  }
  return result;
}

std::future<std::shared_ptr<GPUDeviceLostInfo>> GPUDevice::getLost() {
  return m_lostPromise->get_future();
}
} // namespace rnwgpu
