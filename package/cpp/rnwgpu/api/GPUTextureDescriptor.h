#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {

// TODO: Delete this class and use std::shared_ptr<wgpu::TextureDescriptor>
// instead
class GPUTextureDescriptor {
public:
  wgpu::TextureDescriptor *getInstance() { return &_instance; }

  wgpu::TextureDescriptor _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUTextureDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "size")) {
        auto size = value.getProperty(runtime, "size");

        if (size.isUndefined()) {
          throw std::runtime_error(
              "Property GPUTextureDescriptor::size is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUTextureDescriptor::size is not defined");
      }
      if (value.hasProperty(runtime, "mipLevelCount")) {
        auto mipLevelCount = value.getProperty(runtime, "mipLevelCount");

        if (mipLevelCount.isNumber()) {
          result->_instance.mipLevelCount =
              static_cast<wgpu::IntegerCoordinate>(mipLevelCount.getNumber());
        }
      }
      if (value.hasProperty(runtime, "sampleCount")) {
        auto sampleCount = value.getProperty(runtime, "sampleCount");

        if (sampleCount.isNumber()) {
          result->_instance.sampleCount =
              static_cast<wgpu::Size32>(sampleCount.getNumber());
        }
      }
      if (value.hasProperty(runtime, "dimension")) {
        auto dimension = value.getProperty(runtime, "dimension");
      }
      if (value.hasProperty(runtime, "format")) {
        auto format = value.getProperty(runtime, "format");

        if (format.isUndefined()) {
          throw std::runtime_error(
              "Property GPUTextureDescriptor::format is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUTextureDescriptor::format is not defined");
      }
      if (value.hasProperty(runtime, "usage")) {
        auto usage = value.getProperty(runtime, "usage");

        if (usage.isUndefined()) {
          throw std::runtime_error(
              "Property GPUTextureDescriptor::usage is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUTextureDescriptor::usage is not defined");
      }
      if (value.hasProperty(runtime, "viewFormats")) {
        auto viewFormats = value.getProperty(runtime, "viewFormats");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->_instance.label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::size = %f",
                                 result->_instance.size);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::mipLevelCount = %f",
                                 result->_instance.mipLevelCount);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::sampleCount = %f",
                                 result->_instance.sampleCount);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::dimension = %f",
                                 result->_instance.dimension);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::format = %f",
                                 result->_instance.format);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::usage = %f",
                                 result->_instance.usage);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::viewFormats = %f",
                                 result->_instance.viewFormats);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::label = %f",
                                 result->_instance.label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUTextureDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
