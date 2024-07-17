#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
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

bool conv(wgpu::DeviceDescriptor &out, const GPUDeviceDescriptor &in) {

  return conv(out.requiredFeatures, in.requiredFeatures) &&
         conv(out.requiredLimits, in.requiredLimits) &&
         conv(out.defaultQueue, in.defaultQueue) && conv(out.label, in.label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDeviceDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUDeviceDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDeviceDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // requiredFeatures std::optional<std::vector<wgpu::FeatureName>>
      // requiredLimits std::optional<std::map<std::string, double>>
      // defaultQueue std::optional<std::shared_ptr<GPUQueueDescriptor>>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUDeviceDescriptor> arg) {
    throw std::runtime_error("Invalid GPUDeviceDescriptor::toJSI()");
  }
};

} // namespace margelo