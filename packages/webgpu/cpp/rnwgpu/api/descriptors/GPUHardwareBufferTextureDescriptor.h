#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "JSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// The options of GPUDevice.createHardwareBufferTexture(): the usage of the
// texture (the usages the buffer allows by default, as
// GPUSharedTextureMemory.createTexture() without a descriptor) and a label.
struct GPUHardwareBufferTextureDescriptor {
  std::optional<double> usage;
  std::optional<std::string> label;
};

template <>
struct JSIConverter<
    std::shared_ptr<rnwgpu::GPUHardwareBufferTextureDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUHardwareBufferTextureDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result =
        std::make_shared<rnwgpu::GPUHardwareBufferTextureDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "usage")) {
        auto prop = value.getProperty(runtime, "usage");
        if (prop.isNumber()) {
          result->usage = prop.asNumber();
        }
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        if (!prop.isUndefined()) {
          result->label = JSIConverter<std::optional<std::string>>::fromJSI(
              runtime, prop, false);
        }
      }
    }
    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime & /*runtime*/,
        std::shared_ptr<rnwgpu::GPUHardwareBufferTextureDescriptor> /*arg*/) {
    throw std::runtime_error(
        "Invalid GPUHardwareBufferTextureDescriptor::toJSI()");
  }
};

} // namespace rnwgpu
