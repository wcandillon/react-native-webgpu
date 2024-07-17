#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUQuerySet.h"
#include "GPURenderPassColorAttachment.h"
#include "GPURenderPassDepthStencilAttachment.h"
#include "GPURenderPassTimestampWrites.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURenderPassDescriptor {
  std::vector<std::variant<std::nullptr_t,
                           std::shared_ptr<GPURenderPassColorAttachment>>>
      colorAttachments; // Iterable<GPURenderPassColorAttachment | null>
  std::optional<std::shared_ptr<GPURenderPassDepthStencilAttachment>>
      depthStencilAttachment; // GPURenderPassDepthStencilAttachment
  std::optional<std::shared_ptr<GPUQuerySet>> occlusionQuerySet; // GPUQuerySet
  std::optional<std::shared_ptr<GPURenderPassTimestampWrites>>
      timestampWrites;                // GPURenderPassTimestampWrites
  std::optional<double> maxDrawCount; // GPUSize64
  std::optional<std::string> label;   // string
};

} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPassDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderPassDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPassDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPassDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo