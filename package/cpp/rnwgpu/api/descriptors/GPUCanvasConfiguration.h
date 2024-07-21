#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "GPUDevice.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUCanvasConfiguration {
  // std::shared_ptr<GPUDevice> device; // GPUDevice
  wgpu::TextureFormat format;  // GPUTextureFormat
  std::optional<double> usage; // GPUTextureUsageFlags
  std::optional<std::vector<wgpu::TextureFormat>>
      viewFormats; // Iterable<GPUTextureFormat>
  // std::optional<wgpu::definedColorSpace> colorSpace; // PredefinedColorSpace
  // std::optional<wgpu::CanvasAlphaMode> alphaMode;    // GPUCanvasAlphaMode
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCanvasConfiguration>> {
  static std::shared_ptr<rnwgpu::GPUCanvasConfiguration>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUCanvasConfiguration>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "device")) {
        auto prop = value.getProperty(runtime, "device");
        //        result->device =
        //        JSIConverter<std::shared_ptr<GPUDevice>>::fromJSI(
        //            runtime, prop, false);
      }
      if (value.hasProperty(runtime, "format")) {
        auto prop = value.getProperty(runtime, "format");
        result->format =
            JSIConverter<wgpu::TextureFormat>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "usage")) {
        auto prop = value.getProperty(runtime, "usage");
        result->usage =
            JSIConverter<std::optional<double>>::fromJSI(runtime, prop, false);
      }
      if (value.hasProperty(runtime, "viewFormats")) {
        auto prop = value.getProperty(runtime, "viewFormats");
        result->viewFormats = JSIConverter<
            std::optional<std::vector<wgpu::TextureFormat>>>::fromJSI(runtime,
                                                                      prop,
                                                                      false);
      }
      if (value.hasProperty(runtime, "colorSpace")) {
        auto prop = value.getProperty(runtime, "colorSpace");
        //        result->colorSpace =
        //            JSIConverter<std::optional<wgpu::definedColorSpace>>::fromJSI(
        //                runtime, prop, false);
      }
      if (value.hasProperty(runtime, "alphaMode")) {
        auto prop = value.getProperty(runtime, "alphaMode");
        //        result->alphaMode =
        //            JSIConverter<std::optional<wgpu::CanvasAlphaMode>>::fromJSI(
        //                runtime, prop, false);
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUCanvasConfiguration> arg) {
    throw std::runtime_error("Invalid GPUCanvasConfiguration::toJSI()");
  }
};

} // namespace margelo
