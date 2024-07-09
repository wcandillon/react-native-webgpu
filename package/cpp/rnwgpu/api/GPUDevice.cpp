#include "GPUDevice.h"

namespace rnwgpu {

std::shared_ptr<GPUBuffer> GPUDevice::createBuffer(std::shared_ptr<GPUBufferDescriptor> descriptor) {
    auto aDescriptor = descriptor->getInstance();
    auto result = _instance->CreateBuffer(aDescriptor);
    return std::make_shared<GPUBuffer>(std::make_shared<wgpu::Buffer>(result),
                                       descriptor->label);
}

} // namespace rnwgpu
