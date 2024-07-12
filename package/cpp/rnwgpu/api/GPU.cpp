#include "GPU.h"
#include <utility>

namespace rnwgpu {

std::future<std::shared_ptr<GPUAdapter>>
GPU::requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options) {
  return _async->runAsync([=] {
    auto aOptions = options->getInstance();
    std::shared_ptr<wgpu::Adapter> adapter = std::make_shared<wgpu::Adapter>(nullptr);
    wgpu::RequestAdapterCallbackInfo callback;
    callback.callback = [](WGPURequestAdapterStatus, WGPUAdapter cAdapter, const char *message,
           void *userdata) {
          if (message != nullptr) {
            fprintf(stderr, "%s", message);
            return;
          }
          *static_cast<wgpu::Adapter *>(userdata) =
              wgpu::Adapter::Acquire(cAdapter);
        };
    callback.mode = wgpu::CallbackMode::AllowProcessEvents;
    callback.userdata = adapter.get();
    _instance.RequestAdapter(aOptions, callback);

    return std::make_shared<GPUAdapter>(std::move(adapter), _async);
  });
}


wgpu::TextureFormat GPU::getPreferredCanvasFormat() {
#if defined(__ANDROID__)
  return wgpu::TextureFormat::RGBA8Unorm;
#else
  return wgpu::TextureFormat::BGRA8Unorm;
#endif // defined(__ANDROID__)
}

} // namespace rnwgpu
