#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::ImageCopyExternalImage>> {
  static std::shared_ptr<wgpu::ImageCopyExternalImage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::ImageCopyExternalImage>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "source")) {
        auto source = value.getProperty(runtime, "source");

        if (source.isUndefined()) {
          throw std::runtime_error(
              "Property GPUImageCopyExternalImage::source is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUImageCopyExternalImage::source is not defined");
      }
      if (value.hasProperty(runtime, "origin")) {
        auto origin = value.getProperty(runtime, "origin");
      }
      if (value.hasProperty(runtime, "flipY")) {
        auto flipY = value.getProperty(runtime, "flipY");
      }
    }
    rnwgpu::Logger::logToConsole("GPUImageCopyExternalImage::source = %f",
                                 result->source);
    rnwgpu::Logger::logToConsole("GPUImageCopyExternalImage::origin = %f",
                                 result->origin);
    rnwgpu::Logger::logToConsole("GPUImageCopyExternalImage::flipY = %f",
                                 result->flipY);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::ImageCopyExternalImage> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
