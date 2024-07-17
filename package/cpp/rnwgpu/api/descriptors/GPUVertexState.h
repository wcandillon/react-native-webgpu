#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "webgpu/webgpu_cpp.h"

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

} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUVertexState>> {
  static std::shared_ptr<rnwgpu::GPUVertexState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUVertexState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "buffers")) {
        auto prop = value.getProperty(runtime, "buffers");
        result->buffers =
            JSIConverter::fromJSI<std::optional<std::vector<std::variant<
                std::nullptr_t, std::shared_ptr<GPUVertexBufferLayout>>>>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "module")) {
        auto prop = value.getProperty(runtime, "module");
        result->module =
            JSIConverter::fromJSI<std::shared_ptr<GPUShaderModule>>(
                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "entryPoint")) {
        auto prop = value.getProperty(runtime, "entryPoint");
        result->entryPoint = JSIConverter::fromJSI<std::optional<std::string>>(
            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "constants")) {
        auto prop = value.getProperty(runtime, "constants");
        result->constants =
            JSIConverter::fromJSI<std::optional<std::map<std::string, double>>>(
                runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexState> arg) {
    throw std::runtime_error("Invalid GPUVertexState::toJSI()");
  }
};
} // namespace margelo