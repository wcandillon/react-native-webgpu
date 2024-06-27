#pragma once

#include <future>
#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"
#include <RNFHybridObject.h>
#include <RNFLogger.h>

#include "GPUAdapter.h"
#include "GPURequestAdapterOptions.h"

namespace rnwgpu {

namespace m = margelo;

class GPU : public m::HybridObject {
public:
  explicit GPU(std::shared_ptr<wgpu::Instance> instance)
      : HybridObject("GPU"), _instance(instance) {}

public:
  std::future<std::shared_ptr<GPUAdapter>>
  requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options) {
   
    return std::async(std::launch::async,
                      [=]() { 
 wgpu::RequestAdapterOptions defaultOptions;
    // Create a shared_ptr to GPUAdapter
                                      wgpu::Adapter adapter = nullptr;
          _instance->RequestAdapter(
              nullptr,
              [](WGPURequestAdapterStatus, WGPUAdapter cAdapter,
                 const char *message, void *userdata) {
                if (message != nullptr) {
                  fprintf(stderr, "%s", message);
                  return;
                }
                *static_cast<wgpu::Adapter *>(userdata) =
                    wgpu::Adapter::Acquire(cAdapter);
              },
              &adapter);

                        if (adapter != nullptr) {
                          m::Logger::log("rnwgpu", "Adapter is not null");
                        } else {
                          
                          m::Logger::log("rnwgpu", "Adapter is null!");
                        }
                            wgpu::DeviceDescriptor defaultDescriptor;

                                  wgpu::Device device = nullptr;
          adapter.RequestDevice(
              nullptr,
              [](WGPURequestDeviceStatus, WGPUDevice cDevice,
                 const char *message, void *userdata) {
                if (message != nullptr) {
                  fprintf(stderr, "%s", message);
                  return;
                }
                *static_cast<wgpu::Device *>(userdata) =
                    wgpu::Device::Acquire(cDevice);
              },
              &device);
                        return std::make_shared<GPUAdapter>(); });
  }

  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPU::getBrand, this);
    registerHybridMethod("requestAdapter", &GPU::requestAdapter, this);
  }

private:
  std::shared_ptr<wgpu::Instance> _instance;
};
} // namespace rnwgpu
