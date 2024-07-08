// GPUAdapter.cpp

#include "GPUAdapter.h"
#include <utility>

namespace rnwgpu {

std::future<std::shared_ptr<GPUDevice>>
GPUAdapter::requestDevice(std::shared_ptr<GPUDeviceDescriptor> descriptor) {
  return std::async(std::launch::async, [this, descriptor]() {
 
    auto aDescriptor = descriptor->getInstance();
    wgpu::Device device = nullptr;
    _instance->RequestDevice(
        aDescriptor,
        [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
           const char *message, void *userdata) {
          if (message != nullptr) {
            fprintf(stderr, "%s", message);
            return;
          }
          *static_cast<wgpu::Device *>(userdata) =
              wgpu::Device::Acquire(cDevice);
        },
        &device);

    if (!device) {
      throw std::runtime_error("Failed to request device");
    }
    std::string label = aDescriptor->label ? aDescriptor->label : "";
    return std::make_shared<GPUDevice>(
        std::make_shared<wgpu::Device>(std::move(device)), label);
  });
}

} // namespace rnwgpu
