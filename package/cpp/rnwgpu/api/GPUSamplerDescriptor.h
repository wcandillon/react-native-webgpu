#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<wgpu::SamplerDescriptor>> {
  static std::shared_ptr<wgpu::SamplerDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<wgpu::SamplerDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "addressModeU")) {
        auto addressModeU = value.getProperty(runtime, "addressModeU");
      }
      if (value.hasProperty(runtime, "addressModeV")) {
        auto addressModeV = value.getProperty(runtime, "addressModeV");
      }
      if (value.hasProperty(runtime, "addressModeW")) {
        auto addressModeW = value.getProperty(runtime, "addressModeW");
      }
      if (value.hasProperty(runtime, "magFilter")) {
        auto magFilter = value.getProperty(runtime, "magFilter");
      }
      if (value.hasProperty(runtime, "minFilter")) {
        auto minFilter = value.getProperty(runtime, "minFilter");
      }
      if (value.hasProperty(runtime, "mipmapFilter")) {
        auto mipmapFilter = value.getProperty(runtime, "mipmapFilter");
      }
      if (value.hasProperty(runtime, "lodMinClamp")) {
        auto lodMinClamp = value.getProperty(runtime, "lodMinClamp");

        if (lodMinClamp.isNumber()) {
          result->lodMinClamp = lodMinClamp.getNumber();
        }
      }
      if (value.hasProperty(runtime, "lodMaxClamp")) {
        auto lodMaxClamp = value.getProperty(runtime, "lodMaxClamp");

        if (lodMaxClamp.isNumber()) {
          result->lodMaxClamp = lodMaxClamp.getNumber();
        }
      }
      if (value.hasProperty(runtime, "compare")) {
        auto compare = value.getProperty(runtime, "compare");
      }
      if (value.hasProperty(runtime, "maxAnisotropy")) {
        auto maxAnisotropy = value.getProperty(runtime, "maxAnisotropy");

        if (maxAnisotropy.isNumber()) {
          result->maxAnisotropy = maxAnisotropy.getNumber();
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str.c_str();
        }
      }
    }
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::addressModeU = %f",
                                 result->addressModeU);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::addressModeV = %f",
                                 result->addressModeV);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::addressModeW = %f",
                                 result->addressModeW);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::magFilter = %f",
                                 result->magFilter);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::minFilter = %f",
                                 result->minFilter);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::mipmapFilter = %f",
                                 result->mipmapFilter);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::lodMinClamp = %f",
                                 result->lodMinClamp);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::lodMaxClamp = %f",
                                 result->lodMaxClamp);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::compare = %f",
                                 result->compare);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::maxAnisotropy = %f",
                                 result->maxAnisotropy);
    rnwgpu::Logger::logToConsole("GPUSamplerDescriptor::label = %f",
                                 result->label);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<wgpu::SamplerDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
