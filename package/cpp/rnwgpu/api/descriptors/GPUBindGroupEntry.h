#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUBindingResource.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUBindGroupEntry {
public:
  wgpu::BindGroupEntry *getInstance() { return &_instance; }

  wgpu::BindGroupEntry _instance;
};
} // namespace rnwgpu

namespace margelo {

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupEntry>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupEntry>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "binding")) {
        auto binding = value.getProperty(runtime, "binding");

        if (binding.isNumber()) {
          result->_instance.binding =
              static_cast<wgpu::Index32>(binding.getNumber());
        }

        if (binding.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupEntry::binding is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupEntry::binding is not defined");
      }
      if (value.hasProperty(runtime, "resource")) {
        auto resource = value.getProperty(runtime, "resource");

        if (resource.isObject()) {
          auto val = m::JSIConverter<
              std::shared_ptr<rnwgpu::GPUBindingResource>>::fromJSI(runtime,
                                                                    resource,
                                                                    false);
          result->_instance.resource = val->_instance;
        }

        if (resource.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupEntry::resource is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupEntry::resource is not defined");
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBindGroupEntry> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
