#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUQueueDescriptor.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUDeviceDescriptor {
public:
  wgpu::DeviceDescriptor *getInstance() { return &_instance; }

  wgpu::DeviceDescriptor _instance;

  std::string label;
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
        auto requiredFeatures = value.getProperty(runtime, "requiredFeatures");
      }
      if (value.hasProperty(runtime, "requiredLimits")) {
        auto requiredLimits = value.getProperty(runtime, "requiredLimits");
      }
      if (value.hasProperty(runtime, "defaultQueue")) {
        auto defaultQueue = value.getProperty(runtime, "defaultQueue");

        if (defaultQueue.isObject()) {
          auto val = m::JSIConverter<rnwgpu::GPUQueueDescriptor>::fromJSI(
              runtime, defaultQueue, false);
          result->_instance.defaultQueue = val._instance;
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str;
          result->_instance.label = result->label.c_str();
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUDeviceDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
