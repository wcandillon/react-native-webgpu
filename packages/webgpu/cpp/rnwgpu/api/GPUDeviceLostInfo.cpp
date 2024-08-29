#include "GPUDeviceLostInfo.h"

namespace rnwgpu {
wgpu::DeviceLostReason getReason();
std::string getMessage();
} // namespace rnwgpu