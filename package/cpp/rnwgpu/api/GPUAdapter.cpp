// GPUAdapter.cpp

#include "GPUAdapter.h"
#include <utility>

#include "Convertors.h"

#include "Logger.h"

namespace rnwgpu {

std::future<std::shared_ptr<GPUDevice>> GPUAdapter::requestDevice(
    std::optional<std::shared_ptr<GPUDeviceDescriptor>> descriptor) {
  return std::async(std::launch::async, [this,
                                         descriptor = std::move(descriptor)]() {
    wgpu::Device device = nullptr;
    wgpu::DeviceDescriptor aDescriptor;
    Convertor conv;
    if (!conv(aDescriptor, descriptor)) {
      throw std::runtime_error("Failed to convert GPUDeviceDescriptor");
    }
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
    aDescriptor.deviceLostCallbackInfo = info;
    wgpu::UncapturedErrorCallbackInfo errorInfo;
    errorInfo.userdata = static_cast<void *>(_creationRuntime);
    errorInfo.callback = [](WGPUErrorType type, const char *message,
                            void *userdata) {
      auto creationRuntime = static_cast<jsi::Runtime *>(userdata);
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
      std::string fullMessage = std::string(errorType) + ": " + message;
      Logger::errorToJavascriptConsole(*creationRuntime, fullMessage.c_str());
    };
    aDescriptor.uncapturedErrorCallbackInfo = errorInfo;
    _instance.RequestDevice(
        &aDescriptor,
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
        [](WGPULoggingType type, const char *message, void *userdata) {
          auto creationRuntime = static_cast<jsi::Runtime *>(userdata);
          const char *logLevel = "";
          switch (type) {
          case WGPULoggingType_Warning:
            logLevel = "Warning";
            Logger::warnToJavascriptConsole(*creationRuntime, message);
            break;
          case WGPULoggingType_Error:
            logLevel = "Error";
            Logger::errorToJavascriptConsole(*creationRuntime, message);
            break;
          case WGPULoggingType_Verbose:
            logLevel = "Verbose";
          case WGPULoggingType_Info:
            logLevel = "Info";
          default:
            logLevel = "Unknown";
            Logger::logToConsole("%s: %s", logLevel, message);
          }
        },
        _creationRuntime);
    std::string label =
        descriptor.has_value() ? descriptor.value()->label.value_or("") : "";
    return std::make_shared<GPUDevice>(std::move(device), _async, label);
  });
}

} // namespace rnwgpu
