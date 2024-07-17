#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUQueueDescriptor.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

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

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDeviceDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUDeviceDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDeviceDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "requiredFeatures")) {
        auto prop = value.getProperty(runtime, "requiredFeatures");
        result->requiredFeatures = JSIConverter::fromJSI<
            std::optional<std::vector<wgpu::FeatureName>>>(runtime, prop,
                                                           false);
      }
      if (value.hasProperty(runtime, "requiredLimits")) {
        auto prop = value.getProperty(runtime, "requiredLimits");
        result->requiredLimits =
            JSIConverter::fromJSI<std::optional<std::map<std::string, double>>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "defaultQueue")) {
        auto prop = value.getProperty(runtime, "defaultQueue");
        result->defaultQueue = JSIConverter::fromJSI<
            std::optional<std::shared_ptr<GPUQueueDescriptor>>>(runtime, prop,
                                                                false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter::fromJSI<std::optional<std::string>>(
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