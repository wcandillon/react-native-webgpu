#pragma once

#include <memory>
#include <vector>

#include "webgpu/webgpu_cpp.h"

#include "JSIConverter.h"

#include "GPUDevice.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

struct GPUCanvasConfiguration {
  std::shared_ptr<GPUDevice> device; // GPUDevice
  wgpu::TextureFormat format;        // GPUTextureFormat
  std::optional<double> usage;       // GPUTextureUsageFlags
  std::optional<std::vector<wgpu::TextureFormat>>
      viewFormats; // Iterable<GPUTextureFormat>
  wgpu::CompositeAlphaMode alphaMode = wgpu::CompositeAlphaMode::Opaque;
  wgpu::PresentMode presentMode = wgpu::PresentMode::Fifo;
};

} // namespace rnwgpu

namespace rnwgpu {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUCanvasConfiguration>> {
  static std::shared_ptr<rnwgpu::GPUCanvasConfiguration>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUCanvasConfiguration>();
    if (!outOfBounds && arg.isObject()) {
      auto value = arg.getObject(runtime);
      if (value.hasProperty(runtime, "device")) {
        auto prop = value.getProperty(runtime, "device");
        result->device = JSIConverter<std::shared_ptr<GPUDevice>>::fromJSI(
            runtime, prop, false);
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
      if (value.hasProperty(runtime, "alphaMode")) {
        auto prop = value.getProperty(runtime, "alphaMode")
                        .asString(runtime)
                        .utf8(runtime);
        if (prop == "premultiplied") {
          result->alphaMode = wgpu::CompositeAlphaMode::Premultiplied;
        }
      }
      if (value.hasProperty(runtime, "presentMode")) {
        auto prop = value.getProperty(runtime, "presentMode")
                        .asString(runtime)
                        .utf8(runtime);
        if (prop == "fifo") {
          result->presentMode = wgpu::PresentMode::Fifo;
        } else if (prop == "fifo-relaxed") {
          result->presentMode = wgpu::PresentMode::FifoRelaxed;
        } else if (prop == "immediate") {
          result->presentMode = wgpu::PresentMode::Immediate;
        } else if (prop == "mailbox") {
          result->presentMode = wgpu::PresentMode::Mailbox;
        } else {
          throw std::runtime_error(
              "Invalid GPUCanvasConfiguration.presentMode");
        }
      }
    }

    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUCanvasConfiguration> arg) {
    throw std::runtime_error("Invalid GPUCanvasConfiguration::toJSI()");
  }
};

} // namespace rnwgpu
