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
  // Set device lost callback using new template API
  aDescriptor.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [](const wgpu::Device &device, wgpu::DeviceLostReason reason,
         wgpu::StringView message) {
        const char *lostReason = "";
        switch (reason) {
        case wgpu::DeviceLostReason::Destroyed:
          lostReason = "Destroyed";
          break;
        case wgpu::DeviceLostReason::Unknown:
          lostReason = "Unknown";
          break;
        default:
          lostReason = "Unknown";
        }
        Logger::logToConsole("GPU Device Lost (%s): %s", lostReason, message.data);
      });
  
  // Set uncaptured error callback using new template API
  aDescriptor.SetUncapturedErrorCallback(
      [](const wgpu::Device &device, wgpu::ErrorType type, wgpu::StringView message) {
        const char *errorType = "";
        switch (type) {
        case wgpu::ErrorType::Validation:
          errorType = "Validation";
          break;
        case wgpu::ErrorType::OutOfMemory:
          errorType = "Out of Memory";
          break;
        case wgpu::ErrorType::Internal:
          errorType = "Internal";
          break;
        case wgpu::ErrorType::Unknown:
          errorType = "Unknown";
          break;
        default:
          errorType = "Unknown";
        }
        std::string fullMessage = message.length > 0 ?  std::string(errorType) + ": " +
                                  std::string(message.data, message.length) : "no message";
        // TODO: log error
      });
  _instance.RequestDevice(
      &aDescriptor, wgpu::CallbackMode::AllowProcessEvents,
      [](wgpu::RequestDeviceStatus status, wgpu::Device device,
         wgpu::StringView message, wgpu::Device *userdata) {
           if (message.length) {
             fprintf(stderr, "%s", message.data);
             return;
           }
        if (status == wgpu::RequestDeviceStatus::Success) {
          *userdata = std::move(device);
        }
      },
      &device);

  if (!device) {
    throw std::runtime_error("Failed to request device");
  }
  device.SetLoggingCallback(
      [creationRuntime = _creationRuntime](wgpu::LoggingType type, wgpu::StringView message) {
        const char *logLevel = "";
        switch (type) {
        case wgpu::LoggingType::Warning:
          logLevel = "Warning";
          Logger::warnToJavascriptConsole(*creationRuntime, 
                                          std::string(message.data, message.length));
          break;
        case wgpu::LoggingType::Error:
          logLevel = "Error";
          Logger::errorToJavascriptConsole(*creationRuntime,
                                           std::string(message.data, message.length));
          break;
        case wgpu::LoggingType::Verbose:
          logLevel = "Verbose";
          break;
        case wgpu::LoggingType::Info:
          logLevel = "Info";
          break;
        default:
          logLevel = "Unknown";
          Logger::logToConsole("%s: %.*s", logLevel, 
                               static_cast<int>(message.length), message.data);
        }
      });
  std::string label =
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "";
  promise.set_value(
      std::make_shared<GPUDevice>(std::move(device), _async, label));
  return future;
}

std::unordered_set<std::string> GPUAdapter::getFeatures() {
  wgpu::SupportedFeatures supportedFeatures;
  _instance.GetFeatures(&supportedFeatures);
  std::unordered_set<std::string> result;
  for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
    auto feature = supportedFeatures.features[i];
    std::string name;
    convertEnumToJSUnion(feature, &name);
    if (name != "") {
      result.insert(name);
    }
  }
  return result;
}

std::shared_ptr<GPUSupportedLimits> GPUAdapter::getLimits() {
  wgpu::Limits limits{};
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
