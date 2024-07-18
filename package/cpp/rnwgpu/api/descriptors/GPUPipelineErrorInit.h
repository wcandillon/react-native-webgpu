#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUPipelineErrorInit {
  wgpu::PipelineErrorReason reason; // GPUPipelineErrorReason
};

static bool conv(wgpu::PipelineErrorInit &out,
                 const std::shared_ptr<GPUPipelineErrorInit> &in) {
  out.reason = in->reason;
  return;
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUPipelineErrorInit>> {
  static std::shared_ptr<rnwgpu::GPUPipelineErrorInit>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUPipelineErrorInit>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // reason wgpu::PipelineErrorReason
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUPipelineErrorInit> arg) {
    throw std::runtime_error("Invalid GPUPipelineErrorInit::toJSI()");
  }
};

} // namespace margelo