#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUShaderModule.h"
#include "GPUVertexBufferLayout.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUVertexState {
  std::optional<std::vector<
      std::variant<std::nullptr_t, std::shared_ptr<GPUVertexBufferLayout>>>>
      buffers; // Iterable<GPUVertexBufferLayout | null>
  std::shared_ptr<GPUShaderModule> module; // GPUShaderModule
  std::optional<std::string> entryPoint;   // string
  std::optional<std::map<std::string, double>>
      constants; // Record< string, GPUPipelineConstantValue >
};

static bool conv(wgpu::VertexState &out, GPUVertexState &in) {

  return conv(out.buffers, in.buffers) && conv(out.module, in.module) &&
         conv(out.entryPoint, in.entryPoint) &&
         conv(out.constants, in.constants);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexState>> {
  static std::shared_ptr<rnwgpu::GPUVertexState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUVertexState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // buffers std::optional<std::vector<std::variant<std::nullptr_t,
      // std::shared_ptr<GPUVertexBufferLayout>>>>
      // module std::shared_ptr<GPUShaderModule>
      // entryPoint std::optional<std::string>
      // constants std::optional<std::map<std::string, double>>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexState> arg) {
    throw std::runtime_error("Invalid GPUVertexState::toJSI()");
  }
};

} // namespace margelo