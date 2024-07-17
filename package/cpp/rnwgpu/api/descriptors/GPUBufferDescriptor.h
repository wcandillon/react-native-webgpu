#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBufferDescriptor {
  double size;                          // GPUSize64
  double usage;                         // GPUBufferUsageFlags
  std::optional<bool> mappedAtCreation; // boolean
  std::optional<std::string> label;     // string
};

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBufferDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBufferDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBufferDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "size")) {
        auto prop = value.getProperty(runtime, "size");
        result->size = JSIConverter::fromJSI<double>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "usage")) {
        auto prop = value.getProperty(runtime, "usage");
        result->usage = JSIConverter::fromJSI<double>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "mappedAtCreation")) {
        auto prop = value.getProperty(runtime, "mappedAtCreation");
        result->mappedAtCreation =
            JSIConverter::fromJSI<std::optional<bool>>(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter::fromJSI<std::optional<std::string>>(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBufferDescriptor> arg) {
    throw std::runtime_error("Invalid GPUBufferDescriptor::toJSI()");
  }
};
} // namespace margelo