#include "GPUCanvasContext.h"
#include "Convertors.h"
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
                                               const jsi::Value & /*thisVal*/,
                                               const jsi::Value * /*args*/,
                                               size_t /*count*/) {
  auto prevSize = _surfaceInfo->getConfig();
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();
  auto sizeHasChanged = prevSize.width != width || prevSize.height != height;
  if (sizeHasChanged) {
    _surfaceInfo->reconfigure(width, height);
  }
  auto size = _surfaceInfo->getSize();
  _canvas->setClientWidth(size.width);
  _canvas->setClientHeight(size.height);
  auto texture = _surfaceInfo->getCurrentTexture();

  auto surfaceInfo = _surfaceInfo;
  auto presentCb = [surfaceInfo](jsi::Runtime & /*rt*/,
                                 const jsi::Value & /*thisValue*/,
                                 const jsi::Value * /*args*/,
                                 size_t /*count*/) -> jsi::Value {
    surfaceInfo->present();
    return jsi::Value::undefined();
  };
  auto fn = jsi::Function::createFromHostFunction(
      runtime, jsi::PropNameID::forAscii(runtime, "WebGPUPresent"), 0,
      presentCb);

  // On the main JS runtime (Hermes), schedule the present as a microtask —
  // it runs at end of current task with no display latency.
  // On other runtimes (Worklets), microtasks are disabled, so use
  // setTimeout(fn, 0) which gives the same end-of-task semantics.
  if (&runtime == RNWebGPUManager::getMainJSRuntime()) {
    runtime.queueMicrotask(std::move(fn));
  } else {
    auto setTimeout =
        runtime.global().getPropertyAsFunction(runtime, "setTimeout");
    setTimeout.call(runtime, fn, jsi::Value(0));
  }

  // Pass reportsMemoryPressure=false to avoid triggering spurious Hermes GC
  // cycles every frame since the canvas texture doesn't own the buffer.
  auto gpuTexture = std::make_shared<GPUTexture>(texture, "", false);
  return JSIConverter<std::shared_ptr<GPUTexture>>::toJSI(runtime, gpuTexture);
}

} // namespace rnwgpu
