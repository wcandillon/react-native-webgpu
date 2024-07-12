// GPUAdapter.cpp

#include "GPUAdapter.h"
#include <utility>

namespace rnwgpu {

std::future<std::shared_ptr<GPUDevice>>
GPUAdapter::requestDevice(std::shared_ptr<GPUDeviceDescriptor> descriptor) {
  return std::async(std::launch::async, [this, descriptor]() {
    wgpu::Device device = nullptr;
    auto aDescriptor = descriptor->getInstance();
    _instance.RequestDevice(
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
    return std::make_shared<GPUDevice>(std::move(device), _async,
                                       descriptor->label);
  });
}

} // namespace rnwgpu
