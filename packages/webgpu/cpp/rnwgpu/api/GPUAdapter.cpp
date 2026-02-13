#include "GPUAdapter.h"

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Convertors.h"

#include "GPUFeatures.h"
#include "JSIConverter.h"
#include "WGPULogger.h"

namespace rnwgpu {

async::AsyncTaskHandle GPUAdapter::requestDevice(
    std::optional<std::shared_ptr<GPUDeviceDescriptor>> descriptor) {
  wgpu::DeviceDescriptor aDescriptor;
  Convertor conv;
  if (!conv(aDescriptor, descriptor)) {
    throw std::runtime_error("Failed to convert GPUDeviceDescriptor");
  }
  auto deviceLostBinding = std::make_shared<std::weak_ptr<GPUDevice>>();
  // Set device lost callback using new template API
  aDescriptor.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [deviceLostBinding](const wgpu::Device & /*device*/,
                          wgpu::DeviceLostReason reason,
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
        std::string msg =
            message.length ? std::string(message.data, message.length) : "";
        Logger::logToConsole("GPU Device Lost (%s): %s", lostReason,
                             msg.c_str());
        if (auto deviceHost = deviceLostBinding->lock()) {
          deviceHost->notifyDeviceLost(reason, std::move(msg));
        }
      });

  // Set uncaptured error callback using new template API
  // Note: This callback cannot capture variables, so we use a static registry
  // to look up the GPUDevice from the wgpu::Device handle.
  aDescriptor.SetUncapturedErrorCallback([](const wgpu::Device &device,
                                            wgpu::ErrorType type,
                                            wgpu::StringView message) {
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
    std::string msg =
        message.length > 0 ? std::string(message.data, message.length) : "";
    std::string fullMessage =
        msg.length() > 0 ? std::string(errorType) + ": " + msg : "no message";
    fprintf(stderr, "%s\n", fullMessage.c_str());

    // Look up the GPUDevice from the registry and notify it
    if (auto gpuDevice = GPUDevice::lookupDevice(device.Get())) {
      gpuDevice->notifyUncapturedError(type, std::move(msg));
    }
  });
  std::string label =
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "";

  auto creationRuntime = getCreationRuntime();
  return _async->postTask(
      [this, aDescriptor, descriptor, label = std::move(label),
       deviceLostBinding,
       creationRuntime](const async::AsyncTaskHandle::ResolveFunction &resolve,
                        const async::AsyncTaskHandle::RejectFunction &reject) {
        (void)descriptor;
        _instance.RequestDevice(
            &aDescriptor, wgpu::CallbackMode::AllowProcessEvents,
            [asyncRunner = _async, resolve, reject, label, creationRuntime,
             deviceLostBinding](wgpu::RequestDeviceStatus status,
                                wgpu::Device device,
                                wgpu::StringView message) mutable {
              if (message.length) {
                fprintf(stderr, "%s", message.data);
              }

              if (status != wgpu::RequestDeviceStatus::Success || !device) {
                std::string error =
                    message.length ? std::string(message.data, message.length)
                                   : "Failed to request device";
                reject(std::move(error));
                return;
              }

              device.SetLoggingCallback([creationRuntime](
                                            wgpu::LoggingType type,
                                            wgpu::StringView msg) {
                if (creationRuntime == nullptr) {
                  return;
                }
                const char *logLevel = "";
                switch (type) {
                case wgpu::LoggingType::Warning:
                  logLevel = "Warning";
                  Logger::warnToJavascriptConsole(
                      *creationRuntime, std::string(msg.data, msg.length));
                  break;
                case wgpu::LoggingType::Error:
                  logLevel = "Error";
                  Logger::errorToJavascriptConsole(
                      *creationRuntime, std::string(msg.data, msg.length));
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
                                       static_cast<int>(msg.length), msg.data);
                }
              });

              auto deviceHost = std::make_shared<GPUDevice>(std::move(device),
                                                            asyncRunner, label);
              *deviceLostBinding = deviceHost;

              // Register the device in the static registry so the uncaptured
              // error callback can find it
              GPUDevice::registerDevice(deviceHost->get().Get(), deviceHost);

              resolve([deviceHost = std::move(deviceHost)](
                          jsi::Runtime &runtime) mutable {
                return JSIConverter<std::shared_ptr<GPUDevice>>::toJSI(
                    runtime, deviceHost);
              });
            });
      });
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

} // namespace rnwgpu
