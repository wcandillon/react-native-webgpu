#include "GPUAdapter.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Convertors.h"

#include "GPUFeatures.h"
#include "JSIConverter.h"
#include "RnFeatures.h"
#include "WGPULogger.h"

namespace rnwgpu {

async::AsyncTaskHandle GPUAdapter::requestDevice(
    std::optional<std::shared_ptr<GPUDeviceDescriptor>> descriptor) {
  // Enable the react-native-wgpu "native-texture" umbrella by default, mirroring
  // the web where importExternalTexture is core and needs no feature request.
  // We append the umbrella's backing Dawn features to requiredFeatures so the
  // capability is on without the caller listing it. Two rules keep this safe:
  //   - All-or-nothing: only inject when the adapter supports *every* backing
  //     feature (same semantics as maybeSynthesizeRnNativeTextureFeature). On a
  //     web/fallback adapter the backing set is empty or unsupported, so this is
  //     a no-op and device creation is unaffected.
  //   - Requesting a feature the adapter doesn't support makes RequestDevice
  //     fail, hence the support check below.
  // Callers can still pass "rnwebgpu/native-texture" explicitly; the dedupe
  // keeps that idempotent.
  {
    auto backing = rnNativeTextureBackingFeatures();
    if (!backing.empty()) {
      wgpu::SupportedFeatures supported;
      _instance.GetFeatures(&supported);
      std::unordered_set<wgpu::FeatureName> supportedSet(
          supported.features, supported.features + supported.featureCount);
      bool allSupported = std::all_of(
          backing.begin(), backing.end(),
          [&](wgpu::FeatureName f) { return supportedSet.count(f) > 0; });
      if (allSupported) {
        if (!descriptor.has_value()) {
          descriptor = std::make_shared<GPUDeviceDescriptor>();
        }
        auto &desc = descriptor.value();
        if (!desc->requiredFeatures.has_value()) {
          desc->requiredFeatures = std::vector<wgpu::FeatureName>{};
        }
        auto &features = desc->requiredFeatures.value();
        for (auto f : backing) {
          if (std::find(features.begin(), features.end(), f) ==
              features.end()) {
            features.push_back(f);
          }
        }
      }
    }
  }

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
                        const async::AsyncTaskHandle::RejectFunction &reject)
          -> wgpu::Future {
        // Build a local mutable copy so we can chain Dawn's device toggles.
        // The toggle name strings are owned by `descriptor` (captured above),
        // and the const char* / DawnTogglesDescriptor locals live for the
        // whole synchronous RequestDevice call below, which is when Dawn reads
        // the chained struct.
        wgpu::DeviceDescriptor deviceDesc = aDescriptor;
        wgpu::DawnTogglesDescriptor toggles{};
        std::vector<const char *> enabledToggles;
        std::vector<const char *> disabledToggles;
        if (descriptor.has_value() && descriptor.value()->dawnToggles) {
          const auto &dawnToggles = descriptor.value()->dawnToggles.value();
          if (dawnToggles->enabledToggles) {
            for (const auto &t : dawnToggles->enabledToggles.value()) {
              enabledToggles.push_back(t.c_str());
            }
            toggles.enabledToggleCount = enabledToggles.size();
            toggles.enabledToggles = enabledToggles.data();
          }
          if (dawnToggles->disabledToggles) {
            for (const auto &t : dawnToggles->disabledToggles.value()) {
              disabledToggles.push_back(t.c_str());
            }
            toggles.disabledToggleCount = disabledToggles.size();
            toggles.disabledToggles = disabledToggles.data();
          }
          deviceDesc.nextInChain = &toggles;
        }
        return _instance.RequestDevice(
            &deviceDesc, wgpu::CallbackMode::AllowProcessEvents,
            [context = _async, resolve, reject, label, creationRuntime,
             deviceLostBinding](wgpu::RequestDeviceStatus status,
                                wgpu::Device device, wgpu::StringView message) {
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

              device.SetLoggingCallback(
                  [](wgpu::LoggingType type, wgpu::StringView msg,
                     jsi::Runtime *creationRuntime) {
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
                                           static_cast<int>(msg.length),
                                           msg.data);
                    }
                  },
                  creationRuntime);

              auto deviceHost = std::make_shared<GPUDevice>(std::move(device),
                                                            context, label);
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
  std::unordered_set<wgpu::FeatureName> enabled;
  for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
    auto feature = supportedFeatures.features[i];
    enabled.insert(feature);
    std::string name;
    convertEnumToJSUnion(feature, &name);
    if (name != "") {
      result.insert(name);
    }
  }
  maybeSynthesizeRnNativeTextureFeature(enabled, result);
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
