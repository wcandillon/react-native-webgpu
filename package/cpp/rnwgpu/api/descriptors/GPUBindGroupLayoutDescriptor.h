#pragma once

#include <memory>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"
#include "WGPULogger.h"

#include "GPUBindGroupLayoutEntry.h"
#include "RNFHybridObject.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBindGroupLayoutDescriptor {
  std::vector<std::shared_ptr<GPUBindGroupLayoutEntry>>
      entries;                      // Iterable<GPUBindGroupLayoutEntry>
  std::optional<std::string> label; // string
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupLayoutDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupLayoutDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupLayoutDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "entries")) {
        auto prop = value.getProperty(runtime, "entries");
        result->entries = JSIConverter<std::vector<
            std::shared_ptr<GPUBindGroupLayoutEntry>>>::fromJSI(runtime, prop,
                                                                false);
      }
      if (value.hasProperty(runtime, "label")) {
        auto prop = value.getProperty(runtime, "label");
        result->label = JSIConverter<std::optional<std::string>>::fromJSI(
            runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUBindGroupLayoutDescriptor> arg) {
    throw std::runtime_error("Invalid GPUBindGroupLayoutDescriptor::toJSI()");
  }
};

} // namespace margelo