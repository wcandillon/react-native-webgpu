#include "AndroidSurfaceBridge.h"

namespace rnwgpu {

AndroidSurfaceBridge::AndroidSurfaceBridge(GPUWithLock gpu)
    : _gpu(std::move(gpu.gpu)), SurfaceBridge(gpu.lock), _width(0), _height(0) {
  _config.width = 0;
  _config.height = 0;
}

AndroidSurfaceBridge::~AndroidSurfaceBridge() { _surface = nullptr; }

// ─── JS thread ───────────────────────────────────────────────────

void AndroidSurfaceBridge::configure(wgpu::SurfaceConfiguration &newConfig) {
  std::lock_guard<std::mutex> lock(_mutex);

  _config = newConfig;
  _config.presentMode = wgpu::PresentMode::Fifo;
  _config.usage = _config.usage | wgpu::TextureUsage::CopyDst;
}

wgpu::Texture AndroidSurfaceBridge::getCurrentTexture(int width, int height) {
  std::lock_guard<std::mutex> lock(_mutex);
  _width = width;
  _height = height;

  wgpu::TextureDescriptor desc;
  desc.format = _config.format;
  desc.size.width = width;
  desc.size.height = height;
  desc.usage = wgpu::TextureUsage::RenderAttachment |
               wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
               wgpu::TextureUsage::TextureBinding;
  _texture = _config.device.CreateTexture(&desc);

  return _texture;
}

bool AndroidSurfaceBridge::present() {
  std::lock_guard<std::mutex> lock(_mutex);

  if (_texture) {
    _presentedTexture = _texture;
    _texture = nullptr;
  }
  if (_surface) {
    _copyToSurfaceAndPresent();
  }

  return true;
}

// ─── UI thread (Android-specific) ───────────────────────────────

void AndroidSurfaceBridge::switchToOnscreen(ANativeWindow *nativeWindow,
                                            wgpu::Surface surface) {
  std::lock_guard<std::recursive_mutex> gpuLock(_gpuLock->mutex);
  std::unique_lock<std::mutex> lock(_mutex);
  _nativeWindow = nativeWindow;
  _surface = std::move(surface);
  _copyToSurfaceAndPresent();
}

ANativeWindow *AndroidSurfaceBridge::switchToOffscreen() {
  std::lock_guard<std::recursive_mutex> gpuLock(_gpuLock->mutex);
  std::unique_lock<std::mutex> lock(_mutex);
  auto res = _nativeWindow;
  if (_surface) {
      _surface.Unconfigure();
  }
  _surface = nullptr;
  _nativeWindow = nullptr;
  return res;
}

NativeInfo AndroidSurfaceBridge::getNativeInfo() {
  std::lock_guard<std::mutex> lock(_mutex);
  return {.nativeSurface = static_cast<void *>(_nativeWindow),
          .width = _width,
          .height = _height};
}

void AndroidSurfaceBridge::_copyToSurfaceAndPresent() {
  if (!_config.device || !_presentedTexture) {
    return;
  }

  auto queue = _config.device.GetQueue();
  auto future = queue.OnSubmittedWorkDone(
      wgpu::CallbackMode::WaitAnyOnly,
      [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
  wgpu::FutureWaitInfo waitInfo{future};
  _gpu.WaitAny(1, &waitInfo, UINT64_MAX);

  _config.width = _presentedTexture.GetWidth();
  _config.height = _presentedTexture.GetHeight();
  _surface.Configure(&_config);

  copyTextureToSurfaceAndPresent(_config.device, _presentedTexture, _surface);
}

// Factory
std::shared_ptr<SurfaceBridge> createSurfaceBridge(GPUWithLock gpu) {
  return std::make_shared<AndroidSurfaceBridge>(std::move(gpu));
}

} // namespace rnwgpu
