#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUBlendState.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUColorTargetState {
public:
  wgpu::ColorTargetState *getInstance() { return &_instance; }

  wgpu::ColorTargetState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUColorTargetState>> {
  static std::shared_ptr<rnwgpu::GPUColorTargetState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUColorTargetState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isString()) {
          auto str = format.asString(runtime).utf8(runtime);
          wgpu::TextureFormat enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.format = enumValue;
        }

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUColorTargetState::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUColorTargetState::format is not defined");
      }
      if (value.hasProperty(runtime, "blend")) {
        auto blend = value.getProperty(runtime, "blend");

        if (blend.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUBlendState>>::fromJSI(
                  runtime, blend, false);
          result->_instance.blend = val->getInstance();
        }
      }
      if (value.hasProperty(runtime, "writeMask")) {
        auto writeMask = value.getProperty(runtime, "writeMask");

        if (writeMask.isNumber()) {
          result->_instance.writeMask =
              static_cast<wgpu::ColorWriteMask>(writeMask.getNumber());
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUColorTargetState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
