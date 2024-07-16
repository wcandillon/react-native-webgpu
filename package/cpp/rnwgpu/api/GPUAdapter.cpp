// GPUAdapter.cpp

#include "GPUAdapter.h"
#include <utility>

namespace rnwgpu {

std::future<std::shared_ptr<GPUDevice>>
GPUAdapter::requestDevice(std::shared_ptr<GPUDeviceDescriptor> descriptor) {
  return std::async(std::launch::async, [this, descriptor]() {
    wgpu::Device device = nullptr;
    auto aDescriptor = descriptor->getInstance();
    wgpu::DeviceLostCallbackInfo info = {
        .callback = [](WGPUDevice const *device, WGPUDeviceLostReason reason,
                       char const *message, void *userdata) {
          const char *lostReason = "";
          switch (reason) {
          case WGPUDeviceLostReason_Destroyed:
            lostReason = "Destroyed";
            break;
          case WGPUDeviceLostReason_Unknown:
            lostReason = "Unknown";
            break;
          default:
            lostReason = "Unknown";
          }
          Logger::logToConsole("GPU Device Lost (%s): %s", lostReason, message);
        }};
    aDescriptor->deviceLostCallbackInfo = info;
    wgpu::UncapturedErrorCallbackInfo errorInfo = {
        .callback = [](WGPUErrorType type, const char *message, void *) {
          const char *errorType = "";
          switch (type) {
          case WGPUErrorType_Validation:
            errorType = "Validation";
            break;
          case WGPUErrorType_OutOfMemory:
            errorType = "Out of Memory";
            break;
          case WGPUErrorType_Internal:
            errorType = "Internal";
            break;
          case WGPUErrorType_Unknown:
            errorType = "Unknown";
            break;
          default:
            errorType = "Unknown";
          }
          Logger::logToConsole("GPU Error (%s): %s", errorType, message);
        }};
    aDescriptor->uncapturedErrorCallbackInfo = errorInfo;
    _instance.RequestDevice(
        aDescriptor,
        [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
           const char *message, void *userdata) {
          if (message != nullptr) {
            fprintf(stderr, "%s", message);
            return;
          }
          *static_cast<wgpu::Device *>(userdata) =
              wgpu::Device::Acquire(cDevice);
        },
        &device);

    if (!device) {
      throw std::runtime_error("Failed to request device");
    }
    device.SetLoggingCallback(
        [](WGPULoggingType type, const char *message, void *) {
          const char *logLevel = "";
          switch (type) {
          case WGPULoggingType_Verbose:
            logLevel = "Verbose";
            break;
          case WGPULoggingType_Info:
            logLevel = "Info";
            break;
          case WGPULoggingType_Warning:
            logLevel = "Warning";
            break;
          case WGPULoggingType_Error:
            logLevel = "Error";
            break;
          default:
            logLevel = "Unknown";
          }
          Logger::logToConsole("GPU Log (%s): %s", logLevel, message);
        },
        nullptr);
    return std::make_shared<GPUDevice>(std::move(device), _async,
                                       descriptor->label);
  });
}

} // namespace rnwgpu
