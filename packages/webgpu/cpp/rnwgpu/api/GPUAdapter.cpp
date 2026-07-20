#include "GPUAdapter.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "Convertors.h"

#include "GPUFeatures.h"
#include "JSIConverter.h"
#include "RnFeatures.h"
#include "WGPULogger.h"

namespace rnwgpu {

namespace {

void handleDeviceLog(wgpu::LoggingType type, wgpu::StringView message) {
  try {
    const char *level = "Unknown";
    switch (type) {
    case wgpu::LoggingType::Warning:
      level = "Warning";
      break;
    case wgpu::LoggingType::Error:
      level = "Error";
      break;
    case wgpu::LoggingType::Verbose:
      level = "Verbose";
      break;
    case wgpu::LoggingType::Info:
      level = "Info";
      break;
    default:
      break;
    }

    const std::string text =
        message.length > 0 ? std::string(message.data, message.length) : "";
    Logger::logToConsole("%s: %s", level, text.c_str());
  } catch (...) {
    // Never propagate an allocation/logging error through Dawn's callback.
  }
}

} // namespace

async::AsyncTaskHandle GPUAdapter::requestDevice(
    jsi::Runtime &runtime,
    std::optional<std::shared_ptr<GPUDeviceDescriptor>> descriptor) {
  // Enable the react-native-wgpu "native-texture" umbrella by default,
  // mirroring the web where importExternalTexture is core and needs no feature
  // request. We append the umbrella's backing Dawn features to requiredFeatures
  // so the capability is on without the caller listing it. Two rules keep this
  // safe:
  //   - All-or-nothing: only inject when the adapter supports *every* backing
  //     feature (same semantics as maybeSynthesizeRnNativeTextureFeature). On a
  //     web/fallback adapter the backing set is empty or unsupported, so this
  //     is a no-op and device creation is unaffected.
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
      bool allSupported =
          std::all_of(backing.begin(), backing.end(), [&](wgpu::FeatureName f) {
            return supportedSet.count(f) > 0;
          });
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
        try {
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
        } catch (...) {
          // A spontaneous Dawn callback must never unwind across its ABI.
        }
      });

  // Set uncaptured error callback using new template API
  // Note: This callback cannot capture variables, so we use a static registry
  // to look up the GPUDevice from the wgpu::Device handle.
  aDescriptor.SetUncapturedErrorCallback([](const wgpu::Device &device,
                                            wgpu::ErrorType type,
                                            wgpu::StringView message) {
    try {
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

      // Look up the GPUDevice from the registry and notify it.
      if (auto gpuDevice = GPUDevice::lookupDevice(device.Get())) {
        gpuDevice->notifyUncapturedError(type, std::move(msg));
      }
    } catch (...) {
      // A spontaneous Dawn callback must never unwind across its ABI.
    }
  });
  std::string label =
      descriptor.has_value() ? descriptor.value()->label.value_or("") : "";

  // Post to the CALLING runtime's context so the promise settles on the
  // thread that requested it (see GPUBuffer::mapAsync). The GPUDevice is also
  // bound to this context, honoring the contract that a device belongs to the
  // runtime that requested it.
  auto context = async::RuntimeContext::getOrCreate(runtime, _async->instance(),
                                                    _async->sessionState());
  return context->postTask(
      [this, aDescriptor, descriptor, label = std::move(label),
       deviceLostBinding,
       context](const async::AsyncTaskHandle::ResolveFunction &resolve,
                const async::AsyncTaskHandle::RejectFunction &reject) {
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
          }
          if (dawnToggles->disabledToggles) {
            for (const auto &t : dawnToggles->disabledToggles.value()) {
              disabledToggles.push_back(t.c_str());
            }
          }
        }
// TODO: in the latest version of Dawn, this won't be needed
// (https://issues.chromium.org/issues/42241591)
#if defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR
        // The iOS Simulator only advertises MTLFeatureSet_iOS_GPUFamily2, so
        // Dawn defaults disable_base_instance/disable_base_vertex on and then
        // rejects every draw with a non-zero firstInstance or baseVertex,
        // both core WebGPU. The simulator forwards Metal calls to the host
        // GPU, which does support base vertex/instance drawing, so force the
        // toggles off. Device builds are unaffected: WebGPU-capable iPhones
        // and iPads are all GPUFamily3+. These are device-stage toggles, so
        // they must be chained here rather than on the instance descriptor
        // (instance-stage toggle parsing silently drops them).
        static const char *const kSimulatorDisabledToggles[] = {
            "disable_base_instance", "disable_base_vertex"};
        for (const char *name : kSimulatorDisabledToggles) {
          const bool explicitlyEnabled = std::any_of(
              enabledToggles.begin(), enabledToggles.end(),
              [name](const char *t) { return std::strcmp(t, name) == 0; });
          if (!explicitlyEnabled) {
            disabledToggles.push_back(name);
          }
        }
#endif
        if (!enabledToggles.empty() || !disabledToggles.empty()) {
          toggles.enabledToggleCount = enabledToggles.size();
          toggles.enabledToggles = enabledToggles.data();
          toggles.disabledToggleCount = disabledToggles.size();
          toggles.disabledToggles = disabledToggles.data();
          deviceDesc.nextInChain = &toggles;
        }
        _instance.RequestDevice(
            &deviceDesc, wgpu::CallbackMode::AllowProcessEvents,
            [context, resolve, reject, label,
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

              // Dawn logging is spontaneous and may run after a JS runtime
              // reload. Keep it native-only; never retain/dereference a raw
              // jsi::Runtime from this callback.
              device.SetLoggingCallback(handleDeviceLog);

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
