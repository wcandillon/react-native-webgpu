#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"
#include "WGPULogger.h"

#include "GPUQueueDescriptor.h"
#include "RNFHybridObject.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUDeviceDescriptor {
  std::optional<std::vector<wgpu::FeatureName>>
      requiredFeatures; // Iterable<GPUFeatureName>
  std::optional<std::map<std::string, double>>
      requiredLimits; // Record< string, GPUSize64 >
  std::optional<std::shared_ptr<GPUQueueDescriptor>>
      defaultQueue;                 // GPUQueueDescriptor
  std::optional<std::string> label; // string
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDeviceDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUDeviceDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDeviceDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "requiredFeatures")) {
        auto prop = value.getProperty(runtime, "requiredFeatures");
        result->requiredFeatures = JSIConverter<
            std::optional<std::vector<wgpu::FeatureName>>>::fromJSI(runtime,
                                                                    prop,
                                                                    false);
      }
      if (value.hasProperty(runtime, "requiredLimits")) {
        auto prop = value.getProperty(runtime, "requiredLimits");
        result->requiredLimits =
            JSIConverter<std::optional<std::map<std::string, double>>>::fromJSI(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "defaultQueue")) {
        auto prop = value.getProperty(runtime, "defaultQueue");
        result->defaultQueue =
            JSIConverter<std::optional<std::shared_ptr<GPUQueueDescriptor>>>::
                fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter<std::optional<std::string>>::fromJSI(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUDeviceDescriptor> arg) {
    throw std::runtime_error("Invalid GPUDeviceDescriptor::toJSI()");
  }
};

} // namespace margelo