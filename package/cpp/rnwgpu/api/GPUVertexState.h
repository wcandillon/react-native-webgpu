#pragma once

#include <memory>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

namespace jsi = facebook::jsi;

namespace rnwgpu {
class GPUVertexState {
public:
  wgpu::VertexState *getInstance() { return &_instance; }

  wgpu::VertexState _instance;
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
        auto buffers = value.getProperty(runtime, "buffers");
      }
      if (value.hasProperty(runtime, "module")) {
        auto module = value.getProperty(runtime, "module");

        if (module.isUndefined()) {
          throw std::runtime_error(
              "Property GPUVertexState::module is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUVertexState::module is not defined");
      }
      if (value.hasProperty(runtime, "entryPoint")) {
        auto entryPoint = value.getProperty(runtime, "entryPoint");
      }
      if (value.hasProperty(runtime, "constants")) {
        auto constants = value.getProperty(runtime, "constants");
      }
    }
    rnwgpu::Logger::logToConsole("GPUVertexState::buffers = %f",
                                 result->_instance.buffers);
    rnwgpu::Logger::logToConsole("GPUVertexState::module = %f",
                                 result->_instance.module);
    rnwgpu::Logger::logToConsole("GPUVertexState::entryPoint = %f",
                                 result->_instance.entryPoint);
    rnwgpu::Logger::logToConsole("GPUVertexState::constants = %f",
                                 result->_instance.constants);
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUVertexState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
