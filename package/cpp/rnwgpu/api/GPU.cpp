#include "GPU.h"
#include <utility>

namespace rnwgpu {

std::future<std::shared_ptr<GPUAdapter>>
GPU::requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options) {
  return _async->runAsync([=] {
    auto aOptions = options->getInstance();
    wgpu::Adapter adapter = nullptr;
    auto result = std::make_shared<GPUAdapter>(adapter, _async);
    wgpu::RequestAdapterCallbackInfo callback;
    callback.callback = [](WGPURequestAdapterStatus, WGPUAdapter cAdapter,
                           const char *message, void *userdata) {
      if (message != nullptr) {
        fprintf(stderr, "%s", message);
        return;
      }
      *static_cast<wgpu::Adapter *>(userdata) =
          wgpu::Adapter::Acquire(cAdapter);
    };
    callback.mode = wgpu::CallbackMode::AllowProcessEvents;
    callback.userdata = &(result->_instance);
    _instance.RequestAdapter(aOptions, callback);
    return result;
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
