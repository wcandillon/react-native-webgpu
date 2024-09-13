#include "GPUCanvasContext.h"
#include "Convertors.h"

namespace rnwgpu {

void GPUCanvasContext::configure(std::shared_ptr<GPUCanvasConfiguration> configuration) {
  auto lock = std::unique_lock<std::recursive_mutex>(_mutex);
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
  if (_isPendingTextureToFlush) {
    surfaceConfiguration.usage = surfaceConfiguration.usage | wgpu::TextureUsage::CopyDst;
  }
  surfaceConfiguration.width = _canvas->getWidth();
  surfaceConfiguration.height = _canvas->getHeight();
  _instance.Configure(&surfaceConfiguration);
  _lastConfig = configuration;
  _width = _canvas->getWidth();
  _height = _canvas->getHeight();
}

void GPUCanvasContext::unconfigure() {
  auto lock = std::unique_lock<std::recursive_mutex>(_mutex);
  _lastConfig = nullptr;
  _width = _canvas->getWidth();
  _height = _canvas->getHeight();
  _instance.Unconfigure();
}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
  auto lock = std::unique_lock<std::recursive_mutex>(_mutex);
  // we need to reconfigure if the size of the canvas has changed
  if (_width != _canvas->getWidth() || _height != _canvas->getHeight()) {
    configure(_lastConfig);
  }
  wgpu::SurfaceTexture surfaceTexture;
  _instance.GetCurrentTexture(&surfaceTexture);
  auto texture = surfaceTexture.texture;
  if (texture == nullptr) {
    texture = createMockTexture();
  }
  // Default canvas texture label is ""
  return std::make_shared<GPUTexture>(texture, "");
}

void GPUCanvasContext::present() {
  auto lock = std::unique_lock<std::recursive_mutex>(_mutex);
#ifdef __APPLE__
  dawn::native::metal::WaitForCommandsToBeScheduled(_device.Get());
#endif
  _instance.Present();
}

void GPUCanvasContext::updateInstance(std::shared_ptr<Canvas> surface) {
  auto lock = std::unique_lock<std::recursive_mutex>(_mutex);
  _instance = _platformContext->makeSurface(
    _gpu->get(),
    reinterpret_cast<void *>(surface->getSurface()),
    _width,
    _height
  );
  if (_lastConfig != nullptr) {
    configure(_lastConfig);
  }
}

void GPUCanvasContext::maybeFlushPendingTexture(std::shared_ptr<Canvas> surface) {
  auto lock = std::unique_lock<std::recursive_mutex>(_mutex);
  if (!_isPendingTextureToFlush) {
    return;
  }
  _isPendingTextureToFlush = false;

  wgpu::CommandEncoderDescriptor encoderDesc;
  wgpu::CommandEncoder encoder = _device.CreateCommandEncoder(&encoderDesc);

  wgpu::ImageCopyTexture sourceTexture = {};
  sourceTexture.texture = _pendingTextureToFlush;

  wgpu::ImageCopyTexture destinationTexture = {};
  wgpu::SurfaceTexture surfaceTexture;
  _instance.GetCurrentTexture(&surfaceTexture);
  destinationTexture.texture = surfaceTexture.texture;

  wgpu::Extent3D size = {
    _pendingTextureToFlush.GetWidth(),
    _pendingTextureToFlush.GetHeight(),
    _pendingTextureToFlush.GetDepthOrArrayLayers()
  };

  encoder.CopyTextureToTexture(&sourceTexture, &destinationTexture, &size);

  wgpu::CommandBuffer commands = encoder.Finish();
  wgpu::Queue queue = _device.GetQueue();
  queue.Submit(1, &commands);

  present();
}

wgpu::Texture GPUCanvasContext::createMockTexture() {
  wgpu::TextureDescriptor textureDescriptor;
  textureDescriptor.size.width = _width;
  textureDescriptor.size.height = _height;
  textureDescriptor.format = _lastConfig->format;
  textureDescriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
  auto texture = _device.CreateTexture(&textureDescriptor);
  _pendingTextureToFlush = texture;
  _isPendingTextureToFlush = true;
  return texture;
}

} // namespace rnwgpu
