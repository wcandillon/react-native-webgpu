#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::TextureDescriptor>> {
  static std::shared_ptr<wgpu::TextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::TextureDescriptor>();
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
          result->mipLevelCount =
              static_cast<wgpu::IntegerCoordinate>(mipLevelCount.getNumber());
        }
      }
      if (value.hasProperty(runtime, "sampleCount")) {
        auto sampleCount = value.getProperty(runtime, "sampleCount");

        if (sampleCount.isNumber()) {
          result->sampleCount =
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
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::size = %f",
                                 result->size);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::mipLevelCount = %f",
                                 result->mipLevelCount);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::sampleCount = %f",
                                 result->sampleCount);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::dimension = %f",
                                 result->dimension);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::format = %f",
                                 result->format);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::usage = %f",
                                 result->usage);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::viewFormats = %f",
                                 result->viewFormats);
    rnwgpu::Logger::logToConsole("GPUTextureDescriptor::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::TextureDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
