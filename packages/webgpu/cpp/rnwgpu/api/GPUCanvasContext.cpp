#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "RNWebGPUManager.h"

namespace rnwgpu {

void GPUCanvasContext::configure(
    std::shared_ptr<GPUCanvasConfiguration> configuration) {
  Convertor conv;
  wgpu::SurfaceConfiguration surfaceConfiguration;
  surfaceConfiguration.device = configuration->device->get();
  _device = configuration->device->get();
  if (configuration->viewFormats.has_value()) {
    if (!conv(surfaceConfiguration.viewFormats,
              surfaceConfiguration.viewFormatCount,
              configuration->viewFormats.value())) {
      throw std::runtime_error("Error with SurfaceConfiguration");
    }
  }
  if (!conv(surfaceConfiguration.format, configuration->format)) {
    throw std::runtime_error("Error with SurfaceConfiguration");
  }
  if (!conv(surfaceConfiguration.usage, configuration->usage)) {
    throw std::runtime_error("Error with SurfaceConfiguration");
  }
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();
  surfaceConfiguration.width = width;
  surfaceConfiguration.height = height;
  _surfaceConfiguration = surfaceConfiguration;
  _offscreenSurface->configure(_surfaceConfiguration);
  // Add texture to the surface registry, when the native surface is available,
  // we will copy its content there
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.addSurface(_contextId, nullptr, width, height,
                      _offscreenSurface->getCurrentTexture());
}

void GPUCanvasContext::unconfigure() {
  if (_instance) {
    _instance.Unconfigure();
  } else if (_offscreenSurface) {
    _offscreenSurface->unconfigure();
  }
}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();

  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  // TODO: flush the content of the offscreen surface
  // TODO: delete Java_com_webgpu_WebGPUModule_createSurfaceContext (and on iOS)

  // 1. is a surface no available?
  if (_pristine && _instance == nullptr) {

    auto info = registry.getSurface(_contextId);
    if (info != nullptr && info->surface != nullptr) {
      _instance = _platformContext->makeSurface(_gpu->get(), info->surface,
                                                width, height);
      _surfaceConfiguration.width = width;
      _surfaceConfiguration.height = height;
      _instance.Configure(&_surfaceConfiguration);
      // 1.a flush texture to the onscreen surface
      if (info->texture) {
        wgpu::CommandEncoderDescriptor encoderDesc;
        wgpu::CommandEncoder encoder =
            _device.CreateCommandEncoder(&encoderDesc);

        wgpu::ImageCopyTexture sourceTexture = {};
        sourceTexture.texture = info->texture;

        wgpu::ImageCopyTexture destinationTexture = {};
        wgpu::SurfaceTexture surfaceTexture;
        _instance.GetCurrentTexture(&surfaceTexture);
        destinationTexture.texture = surfaceTexture.texture;

        wgpu::Extent3D size = {sourceTexture.texture.GetWidth(),
                               sourceTexture.texture.GetHeight(),
                               sourceTexture.texture.GetDepthOrArrayLayers()};

        encoder.CopyTextureToTexture(&sourceTexture, &destinationTexture,
                                     &size);

        wgpu::CommandBuffer commands = encoder.Finish();
        wgpu::Queue queue = _device.GetQueue();
        queue.Submit(1, &commands);
        // TODO: info->texture = nullptr;
      }
      _offscreenSurface = nullptr;
    }
  }

  // 2. did the surface resize?
  auto prevWidth = _surfaceConfiguration.width;
  auto prevHeight = _surfaceConfiguration.height;
  auto sizeHasChanged = prevWidth != width || prevHeight != height;
  if (_instance && _pristine && sizeHasChanged) {
    _surfaceConfiguration.width = width;
    _surfaceConfiguration.height = height;
    _instance.Configure(&_surfaceConfiguration);
  }

  _pristine = false;
  // 3. get onscreen texture
  if (_instance) {
    wgpu::SurfaceTexture surfaceTexture;
    _instance.GetCurrentTexture(&surfaceTexture);
    auto texture = surfaceTexture.texture;
    if (texture == nullptr) {
      throw std::runtime_error("Couldn't get current texture");
    }
    return std::make_shared<GPUTexture>(texture, "");
  } else {
    // 4. get offscreen texture
    auto tex = _offscreenSurface->getCurrentTexture();
    return std::make_shared<GPUTexture>(tex, "");
  }
}

void GPUCanvasContext::present() {
#ifdef __APPLE__
  dawn::native::metal::WaitForCommandsToBeScheduled(_device.Get());
#endif
  if (_instance) {
    _instance.Present();
    // We update the client width/height for the next frame
    auto &registry = rnwgpu::SurfaceRegistry::getInstance();
    auto info = registry.getSurface(_contextId);
    _canvas->setClientWidth(info->width);
    _canvas->setClientHeight(info->height);
  }
  _pristine = true;
}

} // namespace rnwgpu
