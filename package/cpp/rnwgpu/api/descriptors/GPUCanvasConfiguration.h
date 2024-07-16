#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUDevice.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUCanvasConfiguration {
public:
  wgpu::CanvasConfiguration *getInstance() { return &_instance; }

  wgpu::CanvasConfiguration _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCanvasConfiguration>> {
  static std::shared_ptr<rnwgpu::GPUCanvasConfiguration>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUCanvasConfiguration>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "device")) {
        auto device = value.getProperty(runtime, "device");

        if (device.isObject() &&
            device.getObject(runtime).isHostObject(runtime)) {
          result->_instance.device =
              device.getObject(runtime)
                  .asHostObject<rnwgpu::GPUDevice>(runtime)
                  ->get();
        }

        if (device.isUndefined()) {
          throw std::runtime_error(
              "Property GPUCanvasConfiguration::device is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::device is not defined");
      }
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
              "Property GPUCanvasConfiguration::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUCanvasConfiguration::format is not defined");
      }
      if (value.hasProperty(runtime, "usage")) {
        auto usage = value.getProperty(runtime, "usage");

        if (usage.isNumber()) {
          result->_instance.usage =
              static_cast<wgpu::TextureUsage>(usage.getNumber());
        }
      }
      if (value.hasProperty(runtime, "viewFormats")) {
        auto viewFormats = value.getProperty(runtime, "viewFormats");
      }
      if (value.hasProperty(runtime, "colorSpace")) {
        auto colorSpace = value.getProperty(runtime, "colorSpace");

        if (colorSpace.isString()) {
          auto str = colorSpace.asString(runtime).utf8(runtime);
          wgpu::definedColorSpace enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.colorSpace = enumValue;
        }
      }
      if (value.hasProperty(runtime, "alphaMode")) {
        auto alphaMode = value.getProperty(runtime, "alphaMode");

        if (alphaMode.isString()) {
          auto str = alphaMode.asString(runtime).utf8(runtime);
          wgpu::CanvasAlphaMode enumValue;
          m::EnumMapper::convertJSUnionToEnum(str, &enumValue);
          result->_instance.alphaMode = enumValue;
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUCanvasConfiguration> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
