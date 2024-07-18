#pragma once

#include <memory>
#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "DescriptorConvertors.h"
#include "GPUBufferBinding.h"
#include "GPUExternalTexture.h"
#include "GPUSampler.h"
#include "GPUTextureView.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUBindGroupEntry {
  double binding; // GPUIndex32
  // std::variant<std::shared_ptr<GPUSampler>, std::shared_ptr<GPUTextureView>,
  //              std::shared_ptr<GPUBufferBinding>,
  //              std::shared_ptr<GPUExternalTexture>>
  //     resource; // GPUBindingResource
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUBindGroupEntry>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUBindGroupEntry>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "binding")) {
        auto prop = value.getProperty(runtime, "binding");
        result->binding = JSIConverter<double>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "resource")) {
        auto prop = value.getProperty(runtime, "resource");
        // result->resource = JSIConverter<std::variant<
        //     std::shared_ptr<GPUSampler>, std::shared_ptr<GPUTextureView>,
        //     std::shared_ptr<GPUBufferBinding>,
        //     std::shared_ptr<GPUExternalTexture>>>::fromJSI(runtime, prop,
        //                                                    false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUBindGroupEntry> arg) {
    throw std::runtime_error("Invalid GPUBindGroupEntry::toJSI()");
  }
};

} // namespace margelo
