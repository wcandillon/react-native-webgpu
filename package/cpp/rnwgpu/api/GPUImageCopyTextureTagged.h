#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyTextureTagged {
public:
  wgpu::ImageCopyTextureTagged *getInstance() { return &_instance; }

  wgpu::ImageCopyTextureTagged _instance;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUImageCopyTextureTagged>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "colorSpace")) {
        auto colorSpace = value.getProperty(runtime, "colorSpace");
      }
      if (value.hasProperty(runtime, "premultipliedAlpha")) {
        auto premultipliedAlpha =
            value.getProperty(runtime, "premultipliedAlpha");
      }
    }
    rnwgpu::Logger::logToConsole("GPUImageCopyTextureTagged::colorSpace = %f",
                                 result->_instance.colorSpace);
    rnwgpu::Logger::logToConsole(
        "GPUImageCopyTextureTagged::premultipliedAlpha = %f",
        result->_instance.premultipliedAlpha);
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUImageCopyTextureTagged> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
