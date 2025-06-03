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
      &aOptions, wgpu::CallbackMode::AllowSpontaneous,
      [](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
         wgpu::StringView message, wgpu::Adapter *userdata) {
        if (message.length) {
          fprintf(stderr, "%s", message.data);
          return;
        }
        if (status == wgpu::RequestAdapterStatus::Success) {
          *userdata = std::move(adapter);
        }
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
  // auto count = _instance.EnumerateWGSLLanguageFeatures(nullptr);
  // std::vector<wgpu::WGSLFeatureName> features(count);
  ///_instance.EnumerateWGSLLanguageFeatures(features.data());
  std::unordered_set<std::string> result;
  //  for (auto feature : features) {
  //    std::string name;
  //    switch (feature) {
  //    case wgpu::WGPUFeatureName::ReadonlyAndReadwriteStorageTextures:
  //      name = "readonly_and_readwrite_storage_textures";
  //      break;
  //    case wgpu::WGPUFeatureName::Packed4x8IntegerDotProduct:
  //      name = "packed_4x8_integer_dot_product";
  //      break;
  //    case wgpu::WGPUFeatureName::UnrestrictedPointerParameters:
  //      name = "unrestricted_pointer_parameters";
  //      break;
  //    case wgpu::WGPUFeatureName::PointerCompositeAccess:
  //      name = "pointer_composite_access";
  //      break;
  //    case wgpu::WGPUFeatureName::ChromiumTestingUnimplemented:
  //      name = "chromium_testing_unimplemented";
  //      break;
  //    case wgpu::WGPUFeatureName::ChromiumTestingUnsafeExperimental:
  //      name = "chromium_testing_unsafe_experimental";
  //      break;
  //    case wgpu::WGPUFeatureName::ChromiumTestingExperimental:
  //      name = "chromium_testing_experimental";
  //      break;
  //    case wgpu::WGPUFeatureName::ChromiumTestingShippedWithKillswitch:
  //      name = "chromium_testing_shipped_with_killswitch";
  //      break;
  //    case wgpu::WGPUFeatureName::ChromiumTestingShipped:
  //      name = "chromium_testing_shipped";
  //      break;
  //    }
  //    result.insert(name);
  //  }
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
