#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "FrameDriver.h"
#include "RNWebGPUManager.h"
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
  auto prevSize = _surfaceInfo->getConfig();
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();
  auto sizeHasChanged = prevSize.width != width || prevSize.height != height;
  if (sizeHasChanged) {
    _surfaceInfo->reconfigure(width, height);
  }
  auto texture = _surfaceInfo->getCurrentTexture();

  // Auto-present: acquiring the current texture schedules a present for this
  // surface at the next vsync (spec-aligned "update the rendering" after the
  // frame). Replaces the old explicit context.present(). Offscreen surfaces
  // have no wgpu::Surface, so skip them (their texture is read back directly).
  auto size = _surfaceInfo->getSize();
  _canvas->setClientWidth(size.width);
  _canvas->setClientHeight(size.height);
  if (_surfaceInfo->hasSurface()) {
    // Phase 2: dispatch the present on the main runtime (the only runtime that
    // owns WebGPU rendering today). Phase 3 will tag this with the *calling*
    // runtime so worklet-runtime rendering (e.g. the Reanimated example)
    // presents on its own JS thread, preserving Dawn surface thread-affinity.
    FrameDriver::getInstance().requestPresent(_contextId, _surfaceInfo,
                                              _gpu->getContext()->scheduler());
  }

  // Pass reportsMemoryPressure=false to avoid triggering spurious Hermes GC
  // cycles every frame since the canvas texture doesn't own the buffer.
  return std::make_shared<GPUTexture>(texture, "", false);
}

} // namespace rnwgpu
