#include "GPUCanvasContext.h"
#include "Convertors.h"
#include "RNWebGPUManager.h"
#include "WGPULogger.h"
#include <memory>

#ifdef __APPLE__
#include <pthread.h>
namespace dawn::native::metal {

void WaitForCommandsToBeScheduled(WGPUDevice device);

}
#endif

#if defined(ANDROID) || defined(__ANDROID__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace rnwgpu {

namespace {

bool isOnMainThread() {
#ifdef __APPLE__
  return pthread_main_np() != 0;
#elif defined(ANDROID) || defined(__ANDROID__)
  return syscall(SYS_gettid) == getpid();
#else
  return true;
#endif
}

std::string threadLabel() {
  return isOnMainThread() ? "main" : "background";
}

void warnIfOffMainThread(const char *fnName) {
  if (!isOnMainThread()) {
    fprintf(stderr,
            "[react-native-wgpu] WARNING: %s called off the main thread. "
            "The module currently only supports main-thread rendering.\n",
            fnName);
    Logger::logToConsole(
        "[react-native-wgpu] WARNING: %s called off the main thread.",
        fnName);
  } else {
    fprintf(stderr, "[react-native-wgpu] %s called on main thread.\n", fnName);
  }
}

} // namespace

void GPUCanvasContext::configure(
    std::shared_ptr<GPUCanvasConfiguration> configuration) {
  warnIfOffMainThread("GPUCanvasContext::configure");
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
  warnIfOffMainThread("GPUCanvasContext::getCurrentTexture");
  if (_hasUnpresentedFrame) {
#ifdef __APPLE__
    dawn::native::metal::WaitForCommandsToBeScheduled(
        _surfaceInfo->getDevice().Get());
#endif
    auto size = _surfaceInfo->getSize();
    _canvas->setClientWidth(size.width);
    _canvas->setClientHeight(size.height);
    _surfaceInfo->present();
    _hasUnpresentedFrame = false;
  }
  auto prevSize = _surfaceInfo->getConfig();
  auto width = _canvas->getWidth();
  auto height = _canvas->getHeight();
  auto sizeHasChanged = prevSize.width != width || prevSize.height != height;
  if (sizeHasChanged) {
    _surfaceInfo->reconfigure(width, height);
  }
  auto texture = _surfaceInfo->getCurrentTexture();
  if (texture == nullptr) {
    auto cfg = _surfaceInfo->getConfig();
    auto status = _surfaceInfo->getLastTextureStatus();
    const char *statusStr = "Unknown";
    switch (status) {
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
      statusStr = "SuccessOptimal";
      break;
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
      statusStr = "SuccessSuboptimal";
      break;
    case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
      statusStr = "Timeout";
      break;
    case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
      statusStr = "Outdated";
      break;
    case wgpu::SurfaceGetCurrentTextureStatus::Lost:
      statusStr = "Lost";
      break;
    case wgpu::SurfaceGetCurrentTextureStatus::Error:
      statusStr = "Error";
      break;
    }
    bool nullSurface = _surfaceInfo->wasLastErrorNullSurface();
    throw std::runtime_error(
        std::string("getCurrentTexture() returned a null texture (status=") +
        statusStr + ", thread=" + threadLabel() +
        ", config.width=" + std::to_string(cfg.width) +
        ", config.height=" + std::to_string(cfg.height) +
        ", config.device=" + (cfg.device != nullptr ? "set" : "null") +
        ", surface=" + (nullSurface ? "NULL" : "set") + ").");
  }
  _hasUnpresentedFrame = true;
  // Pass reportsMemoryPressure=false to avoid triggering spurious Hermes GC
  // cycles every frame since the canvas texture doesn't own the buffer.
  return std::make_shared<GPUTexture>(texture, "", false);
}

} // namespace rnwgpu
