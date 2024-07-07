#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include <RNFHybridObject.h>

#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUSamplerDescriptor {
public:
  wgpu::SamplerDescriptor *getInstance() { return &_instance; }

  wgpu::SamplerDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUSamplerDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUSamplerDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUSamplerDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "addressModeU")) {
        auto addressModeU = value.getProperty(runtime, "addressModeU");
      }
      if (value.hasProperty(runtime, "addressModeV")) {
        auto addressModeV = value.getProperty(runtime, "addressModeV");
      }
      if (value.hasProperty(runtime, "addressModeW")) {
        auto addressModeW = value.getProperty(runtime, "addressModeW");
      }
      if (value.hasProperty(runtime, "magFilter")) {
        auto magFilter = value.getProperty(runtime, "magFilter");
      }
      if (value.hasProperty(runtime, "minFilter")) {
        auto minFilter = value.getProperty(runtime, "minFilter");
      }
      if (value.hasProperty(runtime, "mipmapFilter")) {
        auto mipmapFilter = value.getProperty(runtime, "mipmapFilter");
      }
      if (value.hasProperty(runtime, "lodMinClamp")) {
        auto lodMinClamp = value.getProperty(runtime, "lodMinClamp");
      }
      if (value.hasProperty(runtime, "lodMaxClamp")) {
        auto lodMaxClamp = value.getProperty(runtime, "lodMaxClamp");
      }
      if (value.hasProperty(runtime, "compare")) {
        auto compare = value.getProperty(runtime, "compare");
      }
      if (value.hasProperty(runtime, "maxAnisotropy")) {
        auto maxAnisotropy = value.getProperty(runtime, "maxAnisotropy");
      }
    }
    // else if () {
    // throw std::runtime_error("Expected an object for GPUSamplerDescriptor");
    //}
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUSamplerDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
