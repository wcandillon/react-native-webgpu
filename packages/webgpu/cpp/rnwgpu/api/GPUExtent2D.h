#pragma once

#include <memory>
#include <stdexcept>

#include "JSIConverter.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// A 2D size, as an object ({ width, height }) or an array ([width, height]),
// the two forms the spec gives GPUExtent3D. Both members are required in
// either form: a crop size with a missing one would be empty.
struct GPUExtent2D {
  double width = 0;
  double height = 0;
};

template <> struct JSIConverter<std::shared_ptr<GPUExtent2D>> {
  static std::shared_ptr<GPUExtent2D>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_unique<GPUExtent2D>();
    if (!outOfBounds && arg.isObject()) {
      if (arg.getObject(runtime).isArray(runtime)) {
        auto array = arg.getObject(runtime).asArray(runtime);
        if (array.size(runtime) < 2) {
          throw std::runtime_error(
              "Expected an array of size 2 for GPUExtent2D");
        }
        result->width = array.getValueAtIndex(runtime, 0).asNumber();
        result->height = array.getValueAtIndex(runtime, 1).asNumber();
      } else {
        auto object = arg.getObject(runtime);
        auto width = object.getProperty(runtime, "width");
        auto height = object.getProperty(runtime, "height");
        if (!width.isNumber() || !height.isNumber()) {
          throw std::runtime_error(
              "Expected numbers for the width and height of GPUExtent2D");
        }
        result->width = width.asNumber();
        result->height = height.asNumber();
      }
    } else {
      throw std::runtime_error("Expected an object for GPUExtent2D");
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<GPUExtent2D> arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

} // namespace rnwgpu
