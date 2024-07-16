#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUColorTargetState.h"
#include "GPUShaderModule.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUFragmentState {
public:
  wgpu::FragmentState *getInstance() { return &_instance; }

  wgpu::FragmentState _instance;

  std::string entryPoint;
  std::vector<wgpu::ColorTargetState> targets;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUFragmentState>> {
  static std::shared_ptr<rnwgpu::GPUFragmentState>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUFragmentState>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "targets")) {
        auto targets = value.getProperty(runtime, "targets");

        if (targets.isObject() && targets.asObject(runtime).isArray(runtime)) {
          auto targetArray = targets.asObject(runtime).asArray(runtime);
          size_t targetCount = targetArray.size(runtime);
          result->targets.reserve(targetCount);

          for (size_t i = 0; i < targetCount; i++) {
            auto entry = targetArray.getValueAtIndex(runtime, i);
            auto colorTargetState = JSIConverter<
                std::shared_ptr<rnwgpu::GPUColorTargetState>>::fromJSI(runtime,
                                                                    entry,
                                                                    false);
            result->targets.push_back(colorTargetState->_instance);
          }
          result->_instance.targetCount = static_cast<uint32_t>(targetCount);
          result->_instance.targets = result->targets.data();
        } else if (targets.isUndefined()) {
          throw std::runtime_error(
              "Property GPUFragmentState::targets is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUFragmentState::targets is not defined");
      }
      if (value.hasProperty(runtime, "module")) {
        auto module = value.getProperty(runtime, "module");

        if (module.isObject() &&
            module.getObject(runtime).isHostObject(runtime)) {
          result->_instance.module =
              module.getObject(runtime)
                  .asHostObject<rnwgpu::GPUShaderModule>(runtime)
                  ->get();
        }

        if (module.isUndefined()) {
          throw std::runtime_error(
              "Property GPUFragmentState::module is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUFragmentState::module is not defined");
      }
      if (value.hasProperty(runtime, "entryPoint")) {
        auto entryPoint = value.getProperty(runtime, "entryPoint");

        if (entryPoint.isString()) {
          auto str = entryPoint.asString(runtime).utf8(runtime);
          result->entryPoint = str;
        }
      }
      if (value.hasProperty(runtime, "constants")) {
        auto constants = value.getProperty(runtime, "constants");
      }
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUFragmentState> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
