#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPURenderBundleDescriptor {
public:
  wgpu::RenderBundleDescriptor *getInstance() { return &_instance; }

  wgpu::RenderBundleDescriptor _instance;

  std::string label;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderBundleDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderBundleDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderBundleDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
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
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderBundleDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo