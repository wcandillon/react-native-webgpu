#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "Logger.h"
#include "RNFJSIConverter.h"
#include <RNFHybridObject.h>

#include "GPUBindGroupEntry.h"
#include "GPUBuffer.h"
#include "GPUSampler.h"
#include "GPUTextureView.h"

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
              static_cast<uint32_t>(binding.getNumber());
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
          auto resourceObj = resource.getObject(runtime);

          if (resourceObj.hasProperty(runtime, "buffer")) {
            auto buffer = resourceObj.getProperty(runtime, "buffer");
            if (buffer.isObject() &&
                buffer.getObject(runtime).isHostObject(runtime)) {
              auto gpuBuffer =
                  buffer.getObject(runtime).getHostObject<rnwgpu::GPUBuffer>(
                      runtime);
              result->_instance.buffer = gpuBuffer->get();

              if (resourceObj.hasProperty(runtime, "offset")) {
                auto offset = resourceObj.getProperty(runtime, "offset");
                if (offset.isNumber()) {
                  result->_instance.offset =
                      static_cast<uint64_t>(offset.getNumber());
                }
              }

              if (resourceObj.hasProperty(runtime, "size")) {
                auto size = resourceObj.getProperty(runtime, "size");
                if (size.isNumber()) {
                  result->_instance.size =
                      static_cast<uint64_t>(size.getNumber());
                }
              }
            }
          } else if (resourceObj.hasProperty(runtime, "sampler")) {
            auto sampler = resourceObj.getProperty(runtime, "sampler");
            if (sampler.isObject() &&
                sampler.getObject(runtime).isHostObject(runtime)) {
              auto gpuSampler =
                  sampler.getObject(runtime).getHostObject<rnwgpu::GPUSampler>(
                      runtime);
              result->_instance.sampler = gpuSampler->get();
            }
          } else if (resourceObj.hasProperty(runtime, "textureView")) {
            auto textureView = resourceObj.getProperty(runtime, "textureView");
            if (textureView.isObject() &&
                textureView.getObject(runtime).isHostObject(runtime)) {
              auto gpuTextureView =
                  textureView.getObject(runtime)
                      .getHostObject<rnwgpu::GPUTextureView>(runtime);
              result->_instance.textureView = gpuTextureView->get();
            }
          }
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
