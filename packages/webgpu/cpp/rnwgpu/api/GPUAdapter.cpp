#include "GPUAdapter.h"

#include <string>
#include <utility>
#include <vector>

#include "Convertors.h"

#include "GPUFeatures.h"
#include "WGPULogger.h"

namespace rnwgpu {

std::future<std::shared_ptr<GPUDevice>> GPUAdapter::requestDevice(
    std::optional<std::shared_ptr<GPUDeviceDescriptor>> descriptor) {
  std::promise<std::shared_ptr<GPUDevice>> promise;
  auto future = promise.get_future();
  wgpu::Device device = nullptr;
  wgpu::DeviceDescriptor aDescriptor;
  Convertor conv;
  if (!conv(aDescriptor, descriptor)) {
    throw std::runtime_error("Failed to convert GPUDeviceDescriptor");
  }
  wgpu::DeviceLostCallbackInfo info = {
      .callback = [](WGPUDevice const *device, WGPUDeviceLostReason reason,
                     const WGPUStringView message, void *userdata) {
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
        Logger::logToConsole("GPU Device Lost (%s): %s", lostReason, message.data);
      }};
  aDescriptor.deviceLostCallbackInfo = info;
  wgpu::UncapturedErrorCallbackInfo errorInfo;
  errorInfo.userdata = static_cast<void *>(_creationRuntime);
  errorInfo.callback = [](WGPUErrorType type, const WGPUStringView message,
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
    std::string fullMessage = std::string(errorType) + ": " + message.data;
    Logger::errorToJavascriptConsole(*creationRuntime, fullMessage);
  };
  aDescriptor.uncapturedErrorCallbackInfo = errorInfo;
  _instance.RequestDevice(
      &aDescriptor,
      [](WGPURequestDeviceStatus status, WGPUDevice cDevice,
         const WGPUStringView message, void *userdata) {
        if (message.length) {
          fprintf(stderr, "%s", message.data);
          return;
        }
        *static_cast<wgpu::Device *>(userdata) = wgpu::Device::Acquire(cDevice);
      },
      &device);

  if (!device) {
    throw std::runtime_error("Failed to request device");
  }
  device.SetLoggingCallback(
      [](WGPULoggingType type, const WGPUStringView message, void *userdata) {
        auto creationRuntime = static_cast<jsi::Runtime *>(userdata);
        const char *logLevel = "";
        switch (type) {
        case WGPULoggingType_Warning:
          logLevel = "Warning";
          Logger::warnToJavascriptConsole(*creationRuntime, message.data);
          break;
        case WGPULoggingType_Error:
          logLevel = "Error";
          Logger::errorToJavascriptConsole(*creationRuntime, message.data);
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
  promise.set_value(
      std::make_shared<GPUDevice>(std::move(device), _async, label));
  return future;
}

std::unordered_set<std::string> GPUAdapter::getFeatures() {
  size_t count = _instance.EnumerateFeatures(nullptr);
  std::vector<wgpu::FeatureName> features(count);
  _instance.EnumerateFeatures(features.data());
  std::unordered_set<std::string> result;
  for (auto feature : features) {
    std::string name;
    convertEnumToJSUnion(feature, &name);
    if (name != "") {
      result.insert(name);
    }
  }
  return result;
}

std::shared_ptr<GPUSupportedLimits> GPUAdapter::getLimits() {
  wgpu::SupportedLimits limits{};
  if (!_instance.GetLimits(&limits)) {
    throw std::runtime_error("Failed to get limits");
  }
  return std::make_shared<GPUSupportedLimits>(limits);
}

std::shared_ptr<GPUAdapterInfo> GPUAdapter::getInfo() {
  wgpu::AdapterInfo info = {};
  _instance.GetInfo(&info);
  return std::make_shared<GPUAdapterInfo>(info);
}

bool GPUAdapter::getIsFallbackAdapter() {
  wgpu::AdapterInfo adapterInfo = {};
  _instance.GetInfo(&adapterInfo);
  return adapterInfo.adapterType == wgpu::AdapterType::CPU;
}

} // namespace rnwgpu
