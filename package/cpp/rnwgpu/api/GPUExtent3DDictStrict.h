#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

// TODO: Delete this class and use std::shared_ptr<wgpu::Extent3DDictStrict>
// instead
class GPUExtent3DDictStrict {
public:
  wgpu::Extent3DDictStrict *getInstance() { return &_instance; }

  wgpu::Extent3DDictStrict _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUExtent3DDictStrict>> {
  static std::shared_ptr<rnwgpu::GPUExtent3DDictStrict>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUExtent3DDictStrict>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "depth")) {
        auto depth = value.getProperty(runtime, "depth");

        if (depth.isUndefined()) {
          throw std::runtime_error(
              "Property GPUExtent3DDictStrict::depth is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUExtent3DDictStrict::depth is not defined");
      }
      if (value.hasProperty(runtime, "width")) {
        auto width = value.getProperty(runtime, "width");

        if (width.isUndefined()) {
          throw std::runtime_error(
              "Property GPUExtent3DDictStrict::width is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUExtent3DDictStrict::width is not defined");
      }
      if (value.hasProperty(runtime, "height")) {
        auto height = value.getProperty(runtime, "height");

        if (height.isNumber()) {
          result->_instance.height =
              static_cast<wgpu::IntegerCoordinate>(height.getNumber());
        }
      }
      if (value.hasProperty(runtime, "depthOrArrayLayers")) {
        auto depthOrArrayLayers =
            value.getProperty(runtime, "depthOrArrayLayers");

        if (depthOrArrayLayers.isNumber()) {
          result->_instance.depthOrArrayLayers =
              static_cast<wgpu::IntegerCoordinate>(
                  depthOrArrayLayers.getNumber());
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUExtent3DDictStrict::depth = %f",
                                 result->_instance.depth);
    rnwgpu::Logger::logToConsole("GPUExtent3DDictStrict::width = %f",
                                 result->_instance.width);
    rnwgpu::Logger::logToConsole("GPUExtent3DDictStrict::height = %f",
                                 result->_instance.height);
    rnwgpu::Logger::logToConsole(
        "GPUExtent3DDictStrict::depthOrArrayLayers = %f",
        result->_instance.depthOrArrayLayers);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUExtent3DDictStrict> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
