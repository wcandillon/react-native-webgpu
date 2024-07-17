#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "Convertors.h"
#include "GPUDevice.h"
#include "Logger.h"
#include "RNFHybridObject.h"
#include "RNFJSIConverter.h"

namespace jsi = facebook::jsi;
namespace m = margelo;

namespace rnwgpu {

struct GPUCanvasConfiguration {
  std::shared_ptr<GPUDevice> device; // GPUDevice
  wgpu::TextureFormat format;        // GPUTextureFormat
  std::optional<double> usage;       // GPUTextureUsageFlags
  std::optional<std::vector<wgpu::TextureFormat>>
      viewFormats; // Iterable<GPUTextureFormat>
  std::optional<wgpu::definedColorSpace> colorSpace; // PredefinedColorSpace
  std::optional<wgpu::CanvasAlphaMode> alphaMode;    // GPUCanvasAlphaMode
};

bool conv(wgpu::CanvasConfiguration &out, GPUCanvasConfiguration &in) {

  out.format = in.format;
  return conv(out.device, in.device) && conv(out.usage, in.usage) &&
         conv(out.viewFormats, in.viewFormats) &&
         conv(out.colorSpace, in.colorSpace) &&
         conv(out.alphaMode, in.alphaMode);
}

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu;

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCanvasConfiguration>> {
  static std::shared_ptr<rnwgpu::GPUCanvasConfiguration>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUCanvasConfiguration>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      // device std::shared_ptr<GPUDevice>
      // format wgpu::TextureFormat
      // usage std::optional<double>
      // viewFormats std::optional<std::vector<wgpu::TextureFormat>>
      // colorSpace std::optional<wgpu::definedColorSpace>
      // alphaMode std::optional<wgpu::CanvasAlphaMode>
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUCanvasConfiguration> arg) {
    throw std::runtime_error("Invalid GPUCanvasConfiguration::toJSI()");
  }
};

} // namespace margelo