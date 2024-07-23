// GPUAdapter.cpp

#include "GPUAdapter.h"
#include <string>
#include <utility>

#include "Convertors.h"

#include "Logger.h"

namespace rnwgpu {
static void convertEnumToJSUnion(wgpu::FeatureName inEnum,
                                 std::string *outUnion) {
  switch (inEnum) {
  case wgpu::FeatureName::DepthClipControl:
    *outUnion = "depth-clip-control";
    break;
  case wgpu::FeatureName::Depth32FloatStencil8:
    *outUnion = "depth32float-stencil8";
    break;
  case wgpu::FeatureName::TextureCompressionBC:
    *outUnion = "texture-compression-bc";
    break;
  case wgpu::FeatureName::TextureCompressionETC2:
    *outUnion = "texture-compression-etc2";
    break;
  case wgpu::FeatureName::TextureCompressionASTC:
    *outUnion = "texture-compression-astc";
    break;
  case wgpu::FeatureName::TimestampQuery:
    *outUnion = "timestamp-query";
    break;
  case wgpu::FeatureName::IndirectFirstInstance:
    *outUnion = "indirect-first-instance";
    break;
  case wgpu::FeatureName::ShaderF16:
    *outUnion = "shader-f16";
    break;
  case wgpu::FeatureName::RG11B10UfloatRenderable:
    *outUnion = "rg11b10ufloat-renderable";
    break;
  case wgpu::FeatureName::BGRA8UnormStorage:
    *outUnion = "bgra8unorm-storage";
    break;
  case wgpu::FeatureName::Float32Filterable:
    *outUnion = "float32-filterable";
    break;
  default:
    *outUnion = "";
  }
}

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
  return nullptr;
  // return std::make_shared<GPUAdapterInfo>(info);
}

bool GPUAdapter::getIsFallbackAdapter() {
  wgpu::AdapterProperties adapterProperties = {};
  _instance.GetProperties(&adapterProperties);
  return adapterProperties.adapterType == wgpu::AdapterType::DiscreteGPU;
}

} // namespace rnwgpu
