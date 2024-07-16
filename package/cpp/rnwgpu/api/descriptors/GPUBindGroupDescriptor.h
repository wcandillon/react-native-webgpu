#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUBindGroupEntry.h"
#include "GPUBindGroupLayout.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

class GPUBindGroupDescriptor {
public:
  wgpu::BindGroupDescriptor *getInstance() { return &_instance; }

  wgpu::BindGroupDescriptor _instance;

  std::string label;
  std::vector<wgpu::BindGroupEntry> entries;
};
} // namespace rnwgpu

namespace margelo {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupDescriptor>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupDescriptor>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "layout")) {
        auto layout = value.getProperty(runtime, "layout");

        if (layout.isObject() &&
            layout.getObject(runtime).isHostObject(runtime)) {
          result->_instance.layout =
              layout.getObject(runtime)
                  .asHostObject<rnwgpu::GPUBindGroupLayout>(runtime)
                  ->get();
        }

        if (layout.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupDescriptor::layout is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupDescriptor::layout is not defined");
      }
      if (value.hasProperty(runtime, "entries")) {
        auto entries = value.getProperty(runtime, "entries");

        if (entries.isObject() && entries.asObject(runtime).isArray(runtime)) {
          auto entriesArray = entries.asObject(runtime).asArray(runtime);
          size_t entriesCount = entriesArray.size(runtime);
          result->entries.reserve(entriesCount);

          for (size_t i = 0; i < entriesCount; i++) {
            auto entry = entriesArray.getValueAtIndex(runtime, i);
            auto bindGroupEntry = JSIConverter<
                std::shared_ptr<rnwgpu::GPUBindGroupEntry>>::fromJSI(runtime,
                                                                     entry,
                                                                     false);
            result->entries.push_back(bindGroupEntry->_instance);
          }
          result->_instance.entryCount = static_cast<uint32_t>(entriesCount);
          result->_instance.entries = result->entries.data();
        } else if (entries.isUndefined()) {
          throw std::runtime_error(
              "Property GPUBindGroupDescriptor::entries is required");
        }
      } else {
        throw std::runtime_error(
            "Property GPUBindGroupDescriptor::entries is not defined");
      }
      if (value.hasProperty(runtime, "label")) {
        auto label = value.getProperty(runtime, "label");

        if (label.isString()) {
          auto str = label.asString(runtime).utf8(runtime);
          result->label = str;
          result->_instance.label = result->label.c_str();
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBindGroupDescriptor> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};
} // namespace margelo
