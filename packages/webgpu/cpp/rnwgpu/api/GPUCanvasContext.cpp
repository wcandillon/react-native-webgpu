#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "FrameDriver.h"
#include "RNWebGPUManager.h"
#include <memory>

namespace rnwgpu {

namespace {
// Runtimes whose present is automatic (no ctx.present() needed): the main JS
// runtime and the Reanimated UI runtime. Both are reached correctly by the
// global vsync FrameDriver dispatching through the main runtime's scheduler.
// Dedicated worklet runtimes (createWorkletRuntime, Vision Camera frame
// processors, …) run on their own thread with no safe scheduler hook, so they
// present explicitly via ctx.present().
bool isAutoPresentedRuntime(jsi::Runtime &runtime) {
  if (async::RuntimeContext::get(runtime) != nullptr) {
    return true; // main JS runtime
  }
  // Worklets tags every runtime with a numeric `__RUNTIME_KIND`
  // (worklets::RuntimeKind: ReactNative=1, UI=2, Worker=3). Auto-present only
  // the UI runtime; treat Worker / unknown / untagged as needing ctx.present().
  auto kind = runtime.global().getProperty(runtime, "__RUNTIME_KIND");
  if (kind.isNumber()) {
    constexpr int kRuntimeKindUI = 2;
    return static_cast<int>(kind.asNumber()) == kRuntimeKindUI;
  }
  return false;
}
} // namespace

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
  auto prevSize = _surfaceInfo->getConfig();
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();
  auto sizeHasChanged = prevSize.width != width || prevSize.height != height;
  if (sizeHasChanged) {
    _surfaceInfo->reconfigure(width, height);
  }

  auto texture = _surfaceInfo->getCurrentTexture();

  auto size = _surfaceInfo->getSize();
  _canvas->setClientWidth(size.width);
  _canvas->setClientHeight(size.height);

  // Auto-present on the JS / UI runtime: acquiring the current texture
  // schedules a present for this surface at the next vsync (spec-aligned
  // "update the rendering" after the frame), dispatched through the main
  // runtime's scheduler. Dedicated worklet runtimes instead call ctx.present()
  // explicitly on their own thread. Offscreen surfaces have no wgpu::Surface,
  // so skip them (their texture is read back directly).
  if (_surfaceInfo->hasSurface() && isAutoPresentedRuntime(runtime)) {
    FrameDriver::getInstance().requestPresent(_contextId, _surfaceInfo,
                                              _gpu->getContext()->scheduler());
  }

  // Pass reportsMemoryPressure=false to avoid triggering spurious Hermes GC
  // cycles every frame since the canvas texture doesn't own the buffer.
  auto gpuTexture = std::make_shared<GPUTexture>(texture, "", false);
  return JSIConverter<std::shared_ptr<GPUTexture>>::toJSI(runtime, gpuTexture);
}

jsi::Value GPUCanvasContext::present(jsi::Runtime &runtime,
                                     const jsi::Value & /*thisValue*/,
                                     const jsi::Value * /*args*/,
                                     size_t /*count*/) {
  // Only meaningful on a dedicated worklet runtime, where present can't be
  // automated. On the JS / UI runtime present is automatic, so this is a no-op
  // there — which makes it safe to call from a worklet shared between the UI
  // runtime and a dedicated runtime. Presents synchronously on the calling
  // thread (the one that did getCurrentTexture / submit), preserving Dawn
  // surface thread-affinity.
  if (!isAutoPresentedRuntime(runtime) && _surfaceInfo->hasSurface()) {
    _surfaceInfo->presentFrame();
  }
  return jsi::Value::undefined();
}

} // namespace rnwgpu
