#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUQuerySetDescriptor {
  wgpu::QueryType type;             // GPUQueryType
  double count;                     // GPUSize32
  std::optional<std::string> label; // string
};

static bool conv(wgpu::QuerySetDescriptor &out,
                 const std::shared_ptr<GPUQuerySetDescriptor> &in) {
  out.type = in->type;
  return conv(out.count, in->count) && conv(out.label, in->label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUQuerySetDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUQuerySetDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUQuerySetDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // type wgpu::QueryType
      // count double
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUQuerySetDescriptor> arg) {
    throw std::runtime_error("Invalid GPUQuerySetDescriptor::toJSI()");
  }
};

} // namespace margelo