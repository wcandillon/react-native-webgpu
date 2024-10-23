#include "GPU.h"

#include <utility>
#include <vector>

#include "Convertors.h"

namespace rnwgpu {

std::future<std::variant<std::nullptr_t, std::shared_ptr<GPUAdapter>>>
GPU::requestAdapter(
    std::optional<std::shared_ptr<GPURequestAdapterOptions>> options) {
  std::promise<std::variant<std::nullptr_t, std::shared_ptr<GPUAdapter>>>
      promise;
  auto future = promise.get_future();

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
  wgpu::Adapter adapter = nullptr;
  _instance.RequestAdapter(
      &aOptions,
      [](WGPURequestAdapterStatus, WGPUAdapter cAdapter,
         const WGPUStringView message, void *userdata) {
        if (message.length) {
          fprintf(stderr, "%s", message.data);
          return;
        }
        *static_cast<wgpu::Adapter *>(userdata) =
            wgpu::Adapter::Acquire(cAdapter);
      },
      &adapter);
  if (!adapter) {
    promise.set_value(nullptr);
  } else {
    promise.set_value(std::make_shared<GPUAdapter>(std::move(adapter), _async));
  }
  return future;
}

std::unordered_set<std::string> GPU::getWgslLanguageFeatures() {
  auto count = _instance.EnumerateWGSLLanguageFeatures(nullptr);
  std::vector<wgpu::WGSLFeatureName> features(count);
  _instance.EnumerateWGSLLanguageFeatures(features.data());
  std::unordered_set<std::string> result;
  for (auto feature : features) {
    std::string name;
    switch (feature) {
    case wgpu::WGSLFeatureName::ReadonlyAndReadwriteStorageTextures:
      name = "readonly_and_readwrite_storage_textures";
      break;
    case wgpu::WGSLFeatureName::Packed4x8IntegerDotProduct:
      name = "packed_4x8_integer_dot_product";
      break;
    case wgpu::WGSLFeatureName::UnrestrictedPointerParameters:
      name = "unrestricted_pointer_parameters";
      break;
    case wgpu::WGSLFeatureName::PointerCompositeAccess:
      name = "pointer_composite_access";
      break;
    case wgpu::WGSLFeatureName::ChromiumTestingUnimplemented:
      name = "chromium_testing_unimplemented";
      break;
    case wgpu::WGSLFeatureName::ChromiumTestingUnsafeExperimental:
      name = "chromium_testing_unsafe_experimental";
      break;
    case wgpu::WGSLFeatureName::ChromiumTestingExperimental:
      name = "chromium_testing_experimental";
      break;
    case wgpu::WGSLFeatureName::ChromiumTestingShippedWithKillswitch:
      name = "chromium_testing_shipped_with_killswitch";
      break;
    case wgpu::WGSLFeatureName::ChromiumTestingShipped:
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
