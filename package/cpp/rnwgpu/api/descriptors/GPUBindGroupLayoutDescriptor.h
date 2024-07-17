#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupLayoutEntry.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBindGroupLayoutDescriptor {
  std::vector<std::shared_ptr<GPUBindGroupLayoutEntry>>
      entries;                      // Iterable<GPUBindGroupLayoutEntry>
  std::optional<std::string> label; // string
};

bool conv(wgpu::BindGroupLayoutDescriptor &out,
          const GPUBindGroupLayoutDescriptor &in) {
  out.entryCount = in.entries.size();
  return conv(out.entries, in.entries) && conv(out.label, in.label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

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