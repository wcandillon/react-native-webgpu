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

class GPUPrimitiveState {
public:
  wgpu::PrimitiveState *getInstance() { return &_instance; }

  wgpu::PrimitiveState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUPrimitiveState>> {
  static std::shared_ptr<rnwgpu::GPUPrimitiveState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUPrimitiveState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "topology")) {
        auto topology = value.getProperty(runtime, "topology");

        if (topology.isString()) {
          auto str = topology.asString(runtime).utf8(runtime);
          wgpu::PrimitiveTopology enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.topology = enumValue;
        }
      }
      if (value.hasProperty(runtime, "stripIndexFormat")) {
        auto stripIndexFormat = value.getProperty(runtime, "stripIndexFormat");

        if (stripIndexFormat.isString()) {
          auto str = stripIndexFormat.asString(runtime).utf8(runtime);
          wgpu::IndexFormat enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.stripIndexFormat = enumValue;
        }
      }
      if (value.hasProperty(runtime, "frontFace")) {
        auto frontFace = value.getProperty(runtime, "frontFace");

        if (frontFace.isString()) {
          auto str = frontFace.asString(runtime).utf8(runtime);
          wgpu::FrontFace enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.frontFace = enumValue;
        }
      }
      if (value.hasProperty(runtime, "cullMode")) {
        auto cullMode = value.getProperty(runtime, "cullMode");

        if (cullMode.isString()) {
          auto str = cullMode.asString(runtime).utf8(runtime);
          wgpu::CullMode enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.cullMode = enumValue;
        }
      }
      if (value.hasProperty(runtime, "unclippedDepth")) {
        auto unclippedDepth = value.getProperty(runtime, "unclippedDepth");
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUPrimitiveState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
