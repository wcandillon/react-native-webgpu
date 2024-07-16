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

class GPUBlendComponent {
public:
  wgpu::BlendComponent *getInstance() { return &_instance; }

  wgpu::BlendComponent _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBlendComponent>> {
  static std::shared_ptr<rnwgpu::GPUBlendComponent>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBlendComponent>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "operation")) {
        auto operation = value.getProperty(runtime, "operation");

        if (operation.isString()) {
          auto str = operation.asString(runtime).utf8(runtime);
          wgpu::BlendOperation enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.operation = enumValue;
        }
      }
      if (value.hasProperty(runtime, "srcFactor")) {
        auto srcFactor = value.getProperty(runtime, "srcFactor");

        if (srcFactor.isString()) {
          auto str = srcFactor.asString(runtime).utf8(runtime);
          wgpu::BlendFactor enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.srcFactor = enumValue;
        }
      }
      if (value.hasProperty(runtime, "dstFactor")) {
        auto dstFactor = value.getProperty(runtime, "dstFactor");

        if (dstFactor.isString()) {
          auto str = dstFactor.asString(runtime).utf8(runtime);
          wgpu::BlendFactor enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.dstFactor = enumValue;
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBlendComponent> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
