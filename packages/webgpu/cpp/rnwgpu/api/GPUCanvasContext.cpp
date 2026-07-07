#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "RNWebGPUManager.h"
#include <algorithm>
#include <memory>

namespace rnwgpu {

void GPUCanvasContext::configure(
    std::shared_ptr<GPUCanvasConfiguration> configuration) {
  Convertor conv;
  wgpu::SurfaceConfiguration surfaceConfiguration;
  surfaceConfiguration.device = configuration->device->get();
  if (configuration->viewFormats.has_value()) {
    if (!conv(surfaceConfiguration.viewFormats,
              surfaceConfiguration.viewFormatCount,
              configuration->viewFormats.value())) {
      throw std::runtime_error("Error with SurfaceConfiguration");
    }
  }
  if (!conv(surfaceConfiguration.usage, configuration->usage) ||
      !conv(surfaceConfiguration.format, configuration->format)) {
    throw std::runtime_error("Error with SurfaceConfiguration");
  }

#ifdef __APPLE__
  surfaceConfiguration.alphaMode = configuration->alphaMode;
#endif
  surfaceConfiguration.presentMode = wgpu::PresentMode::Fifo;
  _surfaceInfo->configure(surfaceConfiguration);
}

void GPUCanvasContext::unconfigure() {}

std::shared_ptr<GPUTexture> GPUCanvasContext::getCurrentTexture() {
#if defined(__ANDROID__)
  if (_surfaceInfo->isPoolMode()) {
    // The AHB pool is sized from the canvas drawing buffer (like the swapchain),
    // so the canvas texture always matches the app's other attachments. This
    // (re)allocates the pool when the canvas size changes.
    _surfaceInfo->poolResize(_canvas->getWidth(), _canvas->getHeight());
  } else {
#else
  {
#endif
    auto prevSize = _surfaceInfo->getConfig();
    auto width = _canvas->getWidth();
    auto height = _canvas->getHeight();
    auto sizeHasChanged = prevSize.width != width || prevSize.height != height;
    if (sizeHasChanged) {
      _surfaceInfo->reconfigure(width, height);
    }
  }

  auto texture = _surfaceInfo->getCurrentTexture();
  if (texture == nullptr) {
    // Pool mode can legitimately come up empty (pool not allocated yet, the
    // view detached mid-frame, or a free-slot timeout). Hand JS a transient
    // texture so the frame is skipped gracefully instead of crashing on a null
    // handle in createView().
    auto device = _surfaceInfo->getDevice();
    if (device == nullptr) {
      throw std::runtime_error(
          "getCurrentTexture(): the canvas context is not configured");
    }
    auto config = _surfaceInfo->getConfig();
    wgpu::TextureDescriptor textureDesc;
    textureDesc.usage = wgpu::TextureUsage::RenderAttachment |
                        wgpu::TextureUsage::CopySrc |
                        wgpu::TextureUsage::TextureBinding;
    textureDesc.format = config.format;
    textureDesc.size.width = std::max(_canvas->getWidth(), 1);
    textureDesc.size.height = std::max(_canvas->getHeight(), 1);
    texture = device.CreateTexture(&textureDesc);
  }

  auto size = _surfaceInfo->getSize();
  _canvas->setClientWidth(size.width);
  _canvas->setClientHeight(size.height);

  // Pass reportsMemoryPressure=false to avoid triggering spurious Hermes GC
  // cycles every frame since the canvas texture doesn't own the buffer.
  return std::make_shared<GPUTexture>(texture, "", false);
}

void GPUCanvasContext::present() {
  // Present runs synchronously on the calling thread (the one that did
  // getCurrentTexture / submit), preserving Dawn surface thread-affinity.
  // Required on every runtime (main JS, Reanimated UI, dedicated worklet).
  // Offscreen surfaces have no wgpu::Surface and no pool, so presentFrame() is a
  // no-op there; the AHB pool path has no surface either but must still present.
  if (_surfaceInfo->hasSurface() || _surfaceInfo->isPoolMode()) {
    _surfaceInfo->presentFrame();
  }
}

} // namespace rnwgpu
