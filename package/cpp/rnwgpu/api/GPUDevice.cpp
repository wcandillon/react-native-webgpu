#include "GPUDevice.h"

namespace rnwgpu {

std::shared_ptr<GPUBuffer>
GPUDevice::createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor) {
  auto aDescriptor = descriptor->getInstance();
  auto result = _instance.CreateBuffer(aDescriptor);
  return std::make_shared<GPUBuffer>(result, _async, descriptor->label);
}

std::shared_ptr<GPUQueue> GPUDevice::getQueue() {
  auto result = _instance.GetQueue();
  return std::make_shared<GPUQueue>(result, _async, _label);
};

std::shared_ptr<GPUCommandEncoder> GPUDevice::createCommandEncoder(
    std::shared_ptr<GPUCommandEncoderDescriptor> descriptor) {
  auto aDescriptor = descriptor->getInstance();
  auto result = _instance.CreateCommandEncoder(aDescriptor);
  return std::make_shared<GPUCommandEncoder>(result, _async, descriptor->label);
}

} // namespace rnwgpu
