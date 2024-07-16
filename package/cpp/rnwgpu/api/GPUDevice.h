#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "Unions.h"
#include <RNFHybridObject.h>

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPUBindGroupDescriptor.h"
#include "GPUBuffer.h"
#include "GPUBufferDescriptor.h"
#include "GPUCommandEncoder.h"
#include "GPUCommandEncoderDescriptor.h"
#include "GPUComputePipeline.h"
#include "GPUComputePipelineDescriptor.h"
#include "GPUQueue.h"
#include "GPURenderPipeline.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUSampler.h"
#include "GPUSamplerDescriptor.h"
#include "GPUShaderModule.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"

namespace rnwgpu {

namespace m = margelo;

class GPUDevice : public m::HybridObject {
public:
  explicit GPUDevice(wgpu::Device instance, std::shared_ptr<AsyncRunner> async,
                     std::string label)
      : HybridObject("GPUDevice"), _instance(instance), _async(async),
        _label(label) {
    // TODO: SetDeviceLostCallback is deprecated. Pass the callback in the device descriptor instead.
    // TODO: SetUncapturedErrorCallback is deprecated. Pass the callback in the device descriptor instead.
    // Set up logging callback
    _instance.SetUncapturedErrorCallback(
        [](WGPUErrorType type, const char *message, void *) {
          const char *errorType = "";
          switch (type) {
          case WGPUErrorType_Validation:
            errorType = "Validation";
            break;
          case WGPUErrorType_OutOfMemory:
            errorType = "Out of Memory";
            break;
          case WGPUErrorType_Internal:
            errorType = "Internal";
            break;
          case WGPUErrorType_Unknown:
            errorType = "Unknown";
            break;
          default:
            errorType = "Unknown";
          }
          Logger::logToConsole("GPU Error (%s): %s", errorType, message);
        },
        nullptr);

    // Set up logging callback
    _instance.SetLoggingCallback(
        [](WGPULoggingType type, const char *message, void *) {
          const char *logLevel = "";
          switch (type) {
          case WGPULoggingType_Verbose:
            logLevel = "Verbose";
            break;
          case WGPULoggingType_Info:
            logLevel = "Info";
            break;
          case WGPULoggingType_Warning:
            logLevel = "Warning";
            break;
          case WGPULoggingType_Error:
            logLevel = "Error";
            break;
          default:
            logLevel = "Unknown";
          }
          Logger::logToConsole("GPU Log (%s): %s", logLevel, message);
        },
        nullptr);
    // Set up device lost callback
    _instance.SetDeviceLostCallback(
        [](WGPUDeviceLostReason reason, const char *message, void *) {
          const char *lostReason = "";
          switch (reason) {
          case WGPUDeviceLostReason_Destroyed:
            lostReason = "Destroyed";
            break;
          case WGPUDeviceLostReason_Unknown:
            lostReason = "Unknown";
            break;
          default:
            lostReason = "Unknown";
          }
          Logger::logToConsole("GPU Device Lost (%s): %s", lostReason, message);
        },
        nullptr);
  }

public:
  std::string getBrand() { return _name; }

  void destroy();
  std::shared_ptr<GPUBuffer>
  createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor);
  std::shared_ptr<GPUTexture>
  createTexture(std::shared_ptr<GPUTextureDescriptor> descriptor);
  std::shared_ptr<GPUSampler>
  createSampler(std::shared_ptr<GPUSamplerDescriptor> descriptor);
  std::shared_ptr<GPUBindGroup>
  createBindGroup(std::shared_ptr<GPUBindGroupDescriptor> descriptor);
  std::shared_ptr<GPUShaderModule>
  createShaderModule(std::shared_ptr<GPUShaderModuleDescriptor> descriptor);
  std::shared_ptr<GPUComputePipeline> createComputePipeline(
      std::shared_ptr<GPUComputePipelineDescriptor> descriptor);
  std::shared_ptr<GPURenderPipeline>
  createRenderPipeline(std::shared_ptr<GPURenderPipelineDescriptor> descriptor);
  std::shared_ptr<GPUCommandEncoder>
  createCommandEncoder(std::shared_ptr<GPUCommandEncoderDescriptor> descriptor);

  std::shared_ptr<GPUQueue> getQueue();

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUDevice::getBrand, this);
    registerHybridMethod("destroy", &GPUDevice::destroy, this);
    registerHybridMethod("createBuffer", &GPUDevice::createBuffer, this);
    registerHybridMethod("createTexture", &GPUDevice::createTexture, this);
    registerHybridMethod("createSampler", &GPUDevice::createSampler, this);
    registerHybridMethod("createBindGroup", &GPUDevice::createBindGroup, this);
    registerHybridMethod("createShaderModule", &GPUDevice::createShaderModule,
                         this);
    registerHybridMethod("createComputePipeline",
                         &GPUDevice::createComputePipeline, this);
    registerHybridMethod("createRenderPipeline",
                         &GPUDevice::createRenderPipeline, this);
    registerHybridMethod("createCommandEncoder",
                         &GPUDevice::createCommandEncoder, this);
    registerHybridGetter("queue", &GPUDevice::getQueue, this);
    registerHybridGetter("label", &GPUDevice::getLabel, this);
  }

  inline const wgpu::Device get() { return _instance; }

private:
  wgpu::Device _instance;
  std::shared_ptr<AsyncRunner> _async;
  std::string _label;
};
} // namespace rnwgpu