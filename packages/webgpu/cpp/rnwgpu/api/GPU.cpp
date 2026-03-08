#include "GPU.h"

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Convertors.h"
#include "JSIConverter.h"
#include "rnwgpu/async/JSIMicrotaskDispatcher.h"

namespace rnwgpu {

GPU::GPU(jsi::Runtime &runtime,
         std::vector<std::string> enableToggles,
         std::vector<std::string> disableToggles)
    : NativeObject(CLASS_NAME),
      _enableToggles(std::move(enableToggles)),
      _disableToggles(std::move(disableToggles)) {
  static const auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor instanceDesc{.requiredFeatureCount = 1,
                                        .requiredFeatures = &kTimedWaitAny};

  wgpu::InstanceLimits limits{.timedWaitAnyMaxCount = 64};
  instanceDesc.requiredLimits = &limits;

  // Build Dawn toggles descriptor and chain it if any toggles are specified
  std::vector<const char *> enablePtrs, disablePtrs;
  wgpu::DawnTogglesDescriptor togglesDesc;
  if (!_enableToggles.empty() || !_disableToggles.empty()) {
    for (const auto &s : _enableToggles) enablePtrs.push_back(s.c_str());
    for (const auto &s : _disableToggles) disablePtrs.push_back(s.c_str());
    togglesDesc.enabledToggles = enablePtrs.empty() ? nullptr : enablePtrs.data();
    togglesDesc.enabledToggleCount = enablePtrs.size();
    togglesDesc.disabledToggles = disablePtrs.empty() ? nullptr : disablePtrs.data();
    togglesDesc.disabledToggleCount = disablePtrs.size();
    togglesDesc.nextInChain = instanceDesc.nextInChain;
    instanceDesc.nextInChain = &togglesDesc;
  }

  _instance = wgpu::CreateInstance(&instanceDesc);

  auto dispatcher = std::make_shared<async::JSIMicrotaskDispatcher>(runtime);
  _async = async::AsyncRunner::getOrCreate(runtime, _instance, dispatcher);
}

async::AsyncTaskHandle GPU::requestAdapter(
    std::optional<std::shared_ptr<GPURequestAdapterOptions>> options) {
  wgpu::RequestAdapterOptions aOptions;
  Convertor conv;
  if (!conv(aOptions, options)) {
    throw std::runtime_error("Failed to convert GPUDeviceDescriptor");
  }
#ifdef __APPLE__
  constexpr auto kDefaultBackendType = wgpu::BackendType::Metal;
#else
  constexpr auto kDefaultBackendType = wgpu::BackendType::Vulkan;
#endif
  aOptions.backendType = kDefaultBackendType;

  // Capture toggle strings by value so the lambda owns them
  auto enableToggles = _enableToggles;
  auto disableToggles = _disableToggles;

  return _async->postTask(
      [this, aOptions, enableToggles = std::move(enableToggles),
       disableToggles = std::move(disableToggles)](
          const async::AsyncTaskHandle::ResolveFunction &resolve,
          const async::AsyncTaskHandle::RejectFunction &reject) {
        // Build Dawn toggles chain inside the task so pointers remain valid
        std::vector<const char *> enablePtrs, disablePtrs;
        wgpu::DawnTogglesDescriptor togglesDesc;
        auto localOptions = aOptions;
        if (!enableToggles.empty() || !disableToggles.empty()) {
          for (const auto &s : enableToggles) enablePtrs.push_back(s.c_str());
          for (const auto &s : disableToggles) disablePtrs.push_back(s.c_str());
          togglesDesc.enabledToggles = enablePtrs.empty() ? nullptr : enablePtrs.data();
          togglesDesc.enabledToggleCount = enablePtrs.size();
          togglesDesc.disabledToggles = disablePtrs.empty() ? nullptr : disablePtrs.data();
          togglesDesc.disabledToggleCount = disablePtrs.size();
          togglesDesc.nextInChain = localOptions.nextInChain;
          localOptions.nextInChain = &togglesDesc;
        }
        _instance.RequestAdapter(
            &localOptions, wgpu::CallbackMode::AllowProcessEvents,
            [asyncRunner = _async, resolve,
             reject](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
                     wgpu::StringView message) {
              if (message.length) {
                fprintf(stderr, "%s", message.data);
              }

              if (status == wgpu::RequestAdapterStatus::Success && adapter) {
                auto adapterHost = std::make_shared<GPUAdapter>(
                    std::move(adapter), asyncRunner);
                auto result =
                    std::variant<std::nullptr_t, std::shared_ptr<GPUAdapter>>(
                        adapterHost);
                resolve([result =
                             std::move(result)](jsi::Runtime &runtime) mutable {
                  return JSIConverter<decltype(result)>::toJSI(runtime, result);
                });
              } else {
                auto result =
                    std::variant<std::nullptr_t, std::shared_ptr<GPUAdapter>>(
                        nullptr);
                resolve([result =
                             std::move(result)](jsi::Runtime &runtime) mutable {
                  return JSIConverter<decltype(result)>::toJSI(runtime, result);
                });
              }
            });
      });
}

std::unordered_set<std::string> GPU::getWgslLanguageFeatures() {
  wgpu::SupportedWGSLLanguageFeatures supportedFeatures = {};
  _instance.GetWGSLLanguageFeatures(&supportedFeatures);

  std::unordered_set<std::string> result;
  for (size_t i = 0; i < supportedFeatures.featureCount; i++) {
    wgpu::WGSLLanguageFeatureName feature = supportedFeatures.features[i];
    std::string name;
    switch (feature) {
    case wgpu::WGSLLanguageFeatureName::ReadonlyAndReadwriteStorageTextures:
      name = "readonly_and_readwrite_storage_textures";
      break;
    case wgpu::WGSLLanguageFeatureName::Packed4x8IntegerDotProduct:
      name = "packed_4x8_integer_dot_product";
      break;
    case wgpu::WGSLLanguageFeatureName::UnrestrictedPointerParameters:
      name = "unrestricted_pointer_parameters";
      break;
    case wgpu::WGSLLanguageFeatureName::PointerCompositeAccess:
      name = "pointer_composite_access";
      break;
    case wgpu::WGSLLanguageFeatureName::ChromiumTestingUnimplemented:
      name = "chromium_testing_unimplemented";
      break;
    case wgpu::WGSLLanguageFeatureName::ChromiumTestingUnsafeExperimental:
      name = "chromium_testing_unsafe_experimental";
      break;
    case wgpu::WGSLLanguageFeatureName::ChromiumTestingExperimental:
      name = "chromium_testing_experimental";
      break;
    case wgpu::WGSLLanguageFeatureName::ChromiumTestingShippedWithKillswitch:
      name = "chromium_testing_shipped_with_killswitch";
      break;
    case wgpu::WGSLLanguageFeatureName::ChromiumTestingShipped:
      name = "chromium_testing_shipped";
      break;
    }
    result.insert(name);
  }
  return result;
}

wgpu::TextureFormat GPU::getPreferredCanvasFormat() {
#if defined(__ANDROID__)
  return wgpu::TextureFormat::RGBA8Unorm;
#else
  return wgpu::TextureFormat::BGRA8Unorm;
#endif // defined(__ANDROID__)
}

} // namespace rnwgpu
