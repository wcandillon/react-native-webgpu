#include "GPU.h"
#include <utility>

namespace rnwgpu {

std::future<std::shared_ptr<GPUAdapter>> GPU::requestAdapter(
    std::shared_ptr<GPURequestAdapterOptions> options) {
  return std::async(std::launch::async, [this, options]() {
    wgpu::RequestAdapterOptions wgpuOptions = {};
    // if (options) {
    //     wgpuOptions.powerPreference =
    //     static_cast<WGPUPowerPreference>(options->powerPreference);
    //     wgpuOptions.forceFallbackAdapter = options->forceFallbackAdapter;
    // }

    wgpu::Adapter adapter = nullptr;
    _instance->RequestAdapter(
        nullptr,
        [](WGPURequestAdapterStatus, WGPUAdapter cAdapter, const char *message,
           void *userdata) {
          if (message != nullptr) {
            fprintf(stderr, "%s", message);
            return;
          }
          *static_cast<wgpu::Adapter *>(userdata) =
              wgpu::Adapter::Acquire(cAdapter);
        },
        &adapter);
    // TODO: implement returning null jsi value
    if (!adapter) {
      throw std::runtime_error("Failed to request adapter");
    }

    return std::make_shared<GPUAdapter>(
        std::make_shared<wgpu::Adapter>(std::move(adapter)));
  });
}

} // namespace rnwgpu