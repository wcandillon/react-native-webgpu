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

class GPUStencilFaceState {
public:
  wgpu::StencilFaceState *getInstance() { return &_instance; }

  wgpu::StencilFaceState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUStencilFaceState>> {
  static std::shared_ptr<rnwgpu::GPUStencilFaceState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUStencilFaceState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "compare")) {
        auto compare = value.getProperty(runtime, "compare");

        if (compare.isString()) {
          auto str = compare.asString(runtime).utf8(runtime);
          wgpu::CompareFunction enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.compare = enumValue;
        }
      }
      if (value.hasProperty(runtime, "failOp")) {
        auto failOp = value.getProperty(runtime, "failOp");

        if (failOp.isString()) {
          auto str = failOp.asString(runtime).utf8(runtime);
          wgpu::StencilOperation enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.failOp = enumValue;
        }
      }
      if (value.hasProperty(runtime, "depthFailOp")) {
        auto depthFailOp = value.getProperty(runtime, "depthFailOp");

        if (depthFailOp.isString()) {
          auto str = depthFailOp.asString(runtime).utf8(runtime);
          wgpu::StencilOperation enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.depthFailOp = enumValue;
        }
      }
      if (value.hasProperty(runtime, "passOp")) {
        auto passOp = value.getProperty(runtime, "passOp");

        if (passOp.isString()) {
          auto str = passOp.asString(runtime).utf8(runtime);
          wgpu::StencilOperation enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.passOp = enumValue;
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUStencilFaceState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
