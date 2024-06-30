#pragma once

#include "webgpu/webgpu_cpp.h"
#include <memory>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUImageCopyBuffer {
public:
  wgpu::ImageCopyBuffer *getInstance() { return &_instance; }

private:
  wgpu::ImageCopyBuffer _instance;
};
} // namespace rnwgpu

namespace margelo {

// Object <> Object
template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUImageCopyBuffer>> {
  static std::shared_ptr<rnwgpu::GPUImageCopyBuffer>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto object = arg.getObject(runtime);
    auto result = std::make_unique<rnwgpu::GPUImageCopyBuffer>();
    if (value.hasProperty(runtime, "buffer")) {
      auto buffer = value.getProperty(runtime, "buffer");
      if (buffer.isNumber()) {
        result->_instance.buffer = buffer.getNumber();
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUImageCopyBuffer> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
