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

jsi::Value GPUCanvasContext::getCurrentTexture(jsi::Runtime &runtime,
                                               const jsi::Value & /*thisValue*/,
                                               const jsi::Value * /*args*/,
                                               size_t /*count*/) {
  // Main JS runtime owns a RuntimeContext; worklet runtimes (Reanimated UI /
  // dedicated, Vision Camera frame processors, …) do not.
  auto runtimeContext = async::RuntimeContext::get(runtime);
  const bool isMainRuntime = runtimeContext != nullptr;

  auto prevSize = _surfaceInfo->getConfig();
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();
  auto sizeHasChanged = prevSize.width != width || prevSize.height != height;
  if (sizeHasChanged) {
    _surfaceInfo->reconfigure(width, height);
  }

  // Worklet-runtime auto-present: present the PREVIOUS frame synchronously on
  // this thread, just before acquiring the next texture. By now that frame's
  // submit has already happened (during the previous frame's work), and this
  // runs on the same thread that did getCurrentTexture/submit — preserving Dawn
  // surface thread-affinity. We can't use the UI-thread FrameDriver here, and
  // scheduling onto the worklet runtime's own task queue is unsafe in general
  // (e.g. Vision Camera's queue hops through JNI and crashes off the JS
  // thread), so we present inline at the natural swapchain boundary instead.
  if (!isMainRuntime && _hasUnpresentedFrame && _surfaceInfo->hasSurface()) {
    _surfaceInfo->presentFrame();
    _hasUnpresentedFrame = false;
  }

  auto texture = _surfaceInfo->getCurrentTexture();

  auto size = _surfaceInfo->getSize();
  _canvas->setClientWidth(size.width);
  _canvas->setClientHeight(size.height);

  // Auto-present: acquiring the current texture arranges for this frame to be
  // presented (spec-aligned "update the rendering" after the frame). Replaces
  // the old explicit context.present(). Offscreen surfaces have no
  // wgpu::Surface, so skip them (their texture is read back directly).
  if (_surfaceInfo->hasSurface()) {
    if (isMainRuntime) {
      // Main runtime: drive present from the global vsync FrameDriver (handles
      // one-shot renders too, since it presents the current frame at vsync).
      FrameDriver::getInstance().requestPresent(_contextId, _surfaceInfo,
                                                runtimeContext->scheduler());
    } else {
      // Worklet runtime: present at the next acquire (see above).
      _hasUnpresentedFrame = true;
    }
  }

  // Pass reportsMemoryPressure=false to avoid triggering spurious Hermes GC
  // cycles every frame since the canvas texture doesn't own the buffer.
  auto gpuTexture = std::make_shared<GPUTexture>(texture, "", false);
  return JSIConverter<std::shared_ptr<GPUTexture>>::toJSI(runtime, gpuTexture);
}

} // namespace rnwgpu
