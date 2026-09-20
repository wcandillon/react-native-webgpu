#pragma once

#include <memory>
#include <optional>
#include <string>

#include "JSIConverter.h"

namespace jsi = facebook::jsi;

namespace rnwgpu {

// The `source` argument of GPUQueue.drawElementImageToTexture, modeled on the
// html-in-canvas proposal's GPUDrawElementImageSource. On the web `source` is
// an Element; here it is a native React Native view, identified by its React
// tag. The optional rectangle selects the part of the view to draw, in the
// view's own coordinate space and in density-independent points (React Native
// layout units); it defaults to the whole view.
struct GPUDrawElementImageSource {
  int viewTag = 0;                    // resolved from `source`
  std::optional<double> sourceX;      // float
  std::optional<double> sourceY;      // float
  std::optional<double> sourceWidth;  // float
  std::optional<double> sourceHeight; // float
};

} // namespace rnwgpu

namespace rnwgpu {

template <>
struct JSIConverter<std::shared_ptr<rnwgpu::GPUDrawElementImageSource>> {
  // Resolve `source` to a React tag. Accepted shapes:
  //   - a number: the tag itself (what findNodeHandle returns)
  //   - a React Native host component instance: on the New Architecture these
  //     carry the tag as `__nativeTag` (ReactFabricHostComponent /
  //     ReactNativeElement), which is also what findNodeHandle reads
  //   - a ref object (`{ current }`) wrapping either of the above
  // The JS wrapper on the main runtime additionally runs findNodeHandle so
  // class components and legacy instances resolve too; this native fallback is
  // what worklet runtimes (no findNodeHandle) get.
  static int resolveViewTag(jsi::Runtime &runtime, const jsi::Value &value,
                            int depth = 0) {
    if (value.isNumber()) {
      return static_cast<int>(value.getNumber());
    }
    if (value.isObject() && depth < 2) {
      auto obj = value.getObject(runtime);
      if (obj.hasProperty(runtime, "__nativeTag")) {
        auto tag = obj.getProperty(runtime, "__nativeTag");
        if (tag.isNumber()) {
          return static_cast<int>(tag.getNumber());
        }
      }
      if (obj.hasProperty(runtime, "current")) {
        return resolveViewTag(runtime, obj.getProperty(runtime, "current"),
                              depth + 1);
      }
    }
    throw std::runtime_error(
        "drawElementImageToTexture: `source` must be a React Native view "
        "(a host component ref, its instance, or the tag returned by "
        "findNodeHandle)");
  }

  static std::shared_ptr<rnwgpu::GPUDrawElementImageSource>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<rnwgpu::GPUDrawElementImageSource>();
    if (outOfBounds || !arg.isObject()) {
      throw std::runtime_error(
          "drawElementImageToTexture: expected a source dictionary");
    }
    auto obj = arg.getObject(runtime);
    if (!obj.hasProperty(runtime, "source")) {
      throw std::runtime_error(
          "drawElementImageToTexture: `source.source` is required");
    }
    result->viewTag =
        resolveViewTag(runtime, obj.getProperty(runtime, "source"));
    if (obj.hasProperty(runtime, "sourceX")) {
      result->sourceX = JSIConverter<std::optional<double>>::fromJSI(
          runtime, obj.getProperty(runtime, "sourceX"), false);
    }
    if (obj.hasProperty(runtime, "sourceY")) {
      result->sourceY = JSIConverter<std::optional<double>>::fromJSI(
          runtime, obj.getProperty(runtime, "sourceY"), false);
    }
    if (obj.hasProperty(runtime, "sourceWidth")) {
      result->sourceWidth = JSIConverter<std::optional<double>>::fromJSI(
          runtime, obj.getProperty(runtime, "sourceWidth"), false);
    }
    if (obj.hasProperty(runtime, "sourceHeight")) {
      result->sourceHeight = JSIConverter<std::optional<double>>::fromJSI(
          runtime, obj.getProperty(runtime, "sourceHeight"), false);
    }
    return result;
  }

  static jsi::Value
  toJSI(jsi::Runtime &runtime,
        std::shared_ptr<rnwgpu::GPUDrawElementImageSource> arg) {
    throw std::runtime_error("Invalid GPUDrawElementImageSource::toJSI()");
  }
};

} // namespace rnwgpu
