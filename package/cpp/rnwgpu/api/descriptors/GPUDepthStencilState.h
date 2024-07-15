#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUStencilFaceState.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUDepthStencilState {
public:
  wgpu::DepthStencilState *getInstance() { return &_instance; }

  wgpu::DepthStencilState _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUDepthStencilState>> {
  static std::shared_ptr<rnwgpu::GPUDepthStencilState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDepthStencilState>();
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
              "Property GPUDepthStencilState::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUDepthStencilState::format is not defined");
      }
      if (value.hasProperty(runtime, "depthWriteEnabled")) {
        auto depthWriteEnabled =
            value.getProperty(runtime, "depthWriteEnabled");
        if (depthWriteEnabled.isBool()) {
          result->_instance.depthWriteEnabled = depthWriteEnabled.getBool();
        }
      }
      if (value.hasProperty(runtime, "depthCompare")) {
        auto depthCompare = value.getProperty(runtime, "depthCompare");

        if (depthCompare.isString()) {
          auto str = depthCompare.asString(runtime).utf8(runtime);
          wgpu::CompareFunction enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.depthCompare = enumValue;
        }
      }
      if (value.hasProperty(runtime, "stencilFront")) {
        auto stencilFront = value.getProperty(runtime, "stencilFront");

        if (stencilFront.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUStencilFaceState>>::
                  fromJSI(runtime, stencilFront, false);
          result->_instance.stencilFront = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "stencilBack")) {
        auto stencilBack = value.getProperty(runtime, "stencilBack");

        if (stencilBack.isObject()) {
          auto val =
              m::JSIConverter<std::shared_ptr<rnwgpu::GPUStencilFaceState>>::
                  fromJSI(runtime, stencilBack, false);
          result->_instance.stencilBack = val->_instance;
        }
      }
      if (value.hasProperty(runtime, "stencilReadMask")) {
        auto stencilReadMask = value.getProperty(runtime, "stencilReadMask");

        if (stencilReadMask.isNumber()) {
          result->_instance.stencilReadMask =
              static_cast<uint32_t>(stencilReadMask.getNumber());
        }
      }
      if (value.hasProperty(runtime, "stencilWriteMask")) {
        auto stencilWriteMask = value.getProperty(runtime, "stencilWriteMask");

        if (stencilWriteMask.isNumber()) {
          result->_instance.stencilWriteMask =
              static_cast<uint32_t>(stencilWriteMask.getNumber());
        }
      }
      if (value.hasProperty(runtime, "depthBias")) {
        auto depthBias = value.getProperty(runtime, "depthBias");

        if (depthBias.isNumber()) {
          result->_instance.depthBias =
              static_cast<int32_t>(depthBias.getNumber());
        }
      }
      if (value.hasProperty(runtime, "depthBiasSlopeScale")) {
        auto depthBiasSlopeScale =
            value.getProperty(runtime, "depthBiasSlopeScale");

        if (depthBiasSlopeScale.isNumber()) {
          result->_instance.depthBiasSlopeScale =
              depthBiasSlopeScale.getNumber();
        }
      }
      if (value.hasProperty(runtime, "depthBiasClamp")) {
        auto depthBiasClamp = value.getProperty(runtime, "depthBiasClamp");

        if (depthBiasClamp.isNumber()) {
          result->_instance.depthBiasClamp = depthBiasClamp.getNumber();
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUDepthStencilState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
