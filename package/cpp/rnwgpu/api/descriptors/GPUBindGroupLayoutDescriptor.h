#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
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

static bool conv(wgpu::BindGroupLayoutDescriptor &out,
                 const std::shared_ptr<GPUBindGroupLayoutDescriptor> &in) {
  return conv(out.entries, out.entryCount, in->entries) &&
         conv(out.label, in->label);
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
      // entries std::vector<std::shared_ptr<GPUBindGroupLayoutEntry>>
      // label std::optional<std::string>
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