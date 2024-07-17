#pragma once

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUDepthStencilState.h"
#include "GPUFragmentState.h"
#include "GPUMultisampleState.h"
#include "GPUPipelineLayout.h"
#include "GPUPrimitiveState.h"
#include "GPUVertexState.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPURenderPipelineDescriptor {
  std::shared_ptr<GPUVertexState> vertex; // GPUVertexState
  std::optional<std::shared_ptr<GPUPrimitiveState>>
      primitive; // GPUPrimitiveState
  std::optional<std::shared_ptr<GPUDepthStencilState>>
      depthStencil; // GPUDepthStencilState
  std::optional<std::shared_ptr<GPUMultisampleState>>
      multisample; // GPUMultisampleState
  std::optional<std::shared_ptr<GPUFragmentState>> fragment; // GPUFragmentState
  std::variant<std::nullptr_t, std::shared_ptr<GPUPipelineLayout>>
      layout;                       // | GPUPipelineLayout | GPUAutoLayoutMode
  std::optional<std::string> label; // string
};

static bool conv(wgpu::RenderPipelineDescriptor &out,
                 std::shared_ptr<GPURenderPipelineDescriptor> &in) {

  return conv(out.vertex, in->vertex) && conv(out.primitive, in->primitive) &&
         conv(out.depthStencil, in->depthStencil) &&
         conv(out.multisample, in->multisample) &&
         conv(out.fragment, in->fragment) && conv(out.layout, in->layout) &&
         conv(out.label, in->label);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor>> {
  static std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPURenderPipelineDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // vertex std::shared_ptr<GPUVertexState>
      // primitive std::optional<std::shared_ptr<GPUPrimitiveState>>
      // depthStencil std::optional<std::shared_ptr<GPUDepthStencilState>>
      // multisample std::optional<std::shared_ptr<GPUMultisampleState>>
      // fragment std::optional<std::shared_ptr<GPUFragmentState>>
      // layout std::variant<std::nullptr_t, std::shared_ptr<GPUPipelineLayout>>
      // label std::optional<std::string>
    }

    return result;
  }
  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPURenderPipelineDescriptor> arg) {
    throw std::runtime_error("Invalid GPURenderPipelineDescriptor::toJSI()");
  }
};

} // namespace margelo