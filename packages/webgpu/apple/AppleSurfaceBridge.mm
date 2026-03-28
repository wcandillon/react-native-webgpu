#import <Foundation/Foundation.h>
#include "AppleSurfaceBridge.h"
#include "WGPULogger.h"

namespace dawn::native::metal {
void WaitForCommandsToBeScheduled(WGPUDevice device);
}

namespace rnwgpu {

AppleSurfaceBridge::AppleSurfaceBridge(GPUWithLock gpu)
    : _gpu(std::move(gpu.gpu)), SurfaceBridge(gpu.lock) {
  _config.width = 0;
  _config.height = 0;
}

void AppleSurfaceBridge::configure(wgpu::SurfaceConfiguration &newConfig) {
  std::lock_guard<std::mutex> lock(_mutex);
  // We might need to copy from the offscreen buffer to the surface
  // on resize, or if .present() runs before the native layer is ready.
  newConfig.usage = newConfig.usage | wgpu::TextureUsage::CopyDst;
  newConfig.width = _config.width;
  newConfig.height = _config.height;
  _config = newConfig;
}

NativeInfo AppleSurfaceBridge::getNativeInfo() {
  std::lock_guard<std::mutex> lock(_mutex);
  return {.nativeSurface = _nativeSurface, .width = _width, .height = _height};
}

wgpu::Texture AppleSurfaceBridge::getCurrentTexture(int width, int height) {
  std::lock_guard<std::mutex> lock(_mutex);
  _width = width;
  _height = height;

  if (!_config.device) {
    // The user needs to call configure() before calling the getCurrentTexture().
    return nullptr;
  }

  if (_surface) {
    // If our surface is sized correctly, just use it!
    if (_config.width == width && _config.height == height) {
      // It's safe to update non-size-related properties on the surface. I think.
      // TODO: use other surface properties to determine if we need to reconfigure in the UI thread
      _surface.Configure(&_config);
      wgpu::SurfaceTexture surfTex;
      // It's safe to get the texture without the UI thread roundtrip, only reconfiguration
      // needs to be delegated to the UI thread.
      _surface.GetCurrentTexture(&surfTex);
      return surfTex.texture;
    }
    _resizeSurface(width, height); // Kick off the surface resize in background
  }

  // This can happen if the getCurrentTexture() runs before the UI thread
  // calls prepareToDisplay().
  wgpu::TextureDescriptor textureDesc;
  textureDesc.format = _config.format;
  textureDesc.size.width = width;
  textureDesc.size.height = height;
  textureDesc.usage = wgpu::TextureUsage::RenderAttachment |
                      wgpu::TextureUsage::CopySrc |
                      wgpu::TextureUsage::TextureBinding;
  _renderTargetTexture = _config.device.CreateTexture(&textureDesc);
  return _renderTargetTexture;
}

bool AppleSurfaceBridge::present() {
  std::lock_guard<std::mutex> lock(_mutex);
  if (!_config.device) {
    return false;
  }
  dawn::native::metal::WaitForCommandsToBeScheduled(_config.device.Get());
  // Barrier...
//  auto queue = _config.device.GetQueue();
//  auto future = queue.OnSubmittedWorkDone(
//      wgpu::CallbackMode::WaitAnyOnly,
//      [](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
//  wgpu::FutureWaitInfo waitInfo{future};
//  _gpu.WaitAny(1, &waitInfo, UINT64_MAX);
  if (_renderTargetTexture) {
    _presentedTexture = _renderTargetTexture;
    _renderTargetTexture = nullptr;
    if (_surface) {
      // We were rendering into the texture because the surface was not ready yet
      // or it needed resizing. Check if the current size is compatible with the direct
      // copy.
      int textureWidth = _presentedTexture.GetWidth();
      int textureHeight = _presentedTexture.GetHeight();
      if (_config.width == textureWidth && _config.height == textureHeight) {
        copyTextureToSurfaceAndPresent(_config.device, _presentedTexture, _surface);
      } else {
        // Run the texture resizing in the UI thread asynchronously. It will use the
        // presented texture's dimensions for the size.
        _resizeSurface(0, 0);
      }
    }
  } else if (_surface) {
    // Happy path: rendered onto the surface, no need to keep the presented texture anymore
    _presentedTexture = nullptr;
    _surface.Present();
  }
  return true;
}

void AppleSurfaceBridge::prepareToDisplay(void *nativeSurface, wgpu::Surface surface) {
  // Make sure we prevent the JS thread from racing with this method
  std::lock_guard<std::recursive_mutex> gpuLock(_gpuLock->mutex);
  std::lock_guard<std::mutex> lock(_mutex);

  if (_surface) {
    Logger::logToConsole("Surface assigned multiple times, should never happen");
    return;
  }

  _nativeSurface = nativeSurface; // For nativeInfo only
  _surface = std::move(surface);
  if (_presentedTexture) {
    _doSurfaceConfiguration(0, 0); // Use the presented texture's dimensions
  }
}

void AppleSurfaceBridge::_doSurfaceConfiguration(int width, int height) {
  if (!_config.device || !_surface) {
     return;
  }
  if (_presentedTexture && (width == 0 || height == 0)) {
    width = _presentedTexture.GetWidth();
    height = _presentedTexture.GetHeight();
  }
  if (width <= 0 || height <= 0) {
    // The presented surface has disappeared since the time we were scheduled.
    // It's perfectly fine! This means that the bridge switched into backing surface mode.
    return;
  }
  if (_config.width == width && _config.height == height) {
    return;
  }

  dawn::native::metal::WaitForCommandsToBeScheduled(_config.device.Get());

  _config.width = width;
  _config.height = height;
  _surface.Configure(&_config); // We're in the UI thread, it's safe.
  if (_presentedTexture && _presentedTexture.GetWidth() == width &&
      _presentedTexture.GetHeight() == height) {
    // We have a compatible texture. So copy it to the surface.
    copyTextureToSurfaceAndPresent(_config.device, _presentedTexture, _surface);
    // Don't delete the backing texture in case we want to redisplay it
  }
}

void AppleSurfaceBridge::_resizeSurface(int width, int height) {
  // Make sure that we live long enough for the dispatch to run
  auto self = this->shared_from_this();

  dispatch_async(dispatch_get_main_queue(), ^{
    std::lock_guard<std::recursive_mutex> gpuGuard(self->_gpuLock->mutex);
    std::lock_guard<std::mutex> lock(self->_mutex);
    self->_doSurfaceConfiguration(width, height);
  });
}

// Factory
std::shared_ptr<SurfaceBridge> createSurfaceBridge(GPUWithLock gpu) {
  return std::make_shared<AppleSurfaceBridge>(std::move(gpu));
}

} // namespace rnwgpu
