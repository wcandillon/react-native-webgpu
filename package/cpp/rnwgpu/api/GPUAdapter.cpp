// GPUAdapter.cpp

#include "GPUAdapter.h"
#include <utility>

namespace rnwgpu {

std::future<std::shared_ptr<GPUDevice>>
GPUAdapter::requestDevice(std::shared_ptr<wgpu::DeviceDescriptor> aDescriptor) {
  std::string label;
  if (aDescriptor->label) {
    label = std::string(aDescriptor->label);
  } else {
    label = "";
  }
  return std::async(std::launch::async, [this, aDescriptor, label]() {
    wgpu::Device device = nullptr;
    _instance->RequestDevice(
        aDescriptor.get(),
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
    return std::make_shared<GPUDevice>(
        std::make_shared<wgpu::Device>(std::move(device)), label);
  });
}

} // namespace rnwgpu
