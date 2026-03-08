#pragma once
#include <optional>
#include <string>
#include <vector>
#include "JSIConverter.h"

namespace rnwgpu {

struct GPUDawnTogglesDescriptor {
  std::optional<std::vector<std::string>> enable;
  std::optional<std::vector<std::string>> disable;
};

template <>
struct JSIConverter<std::shared_ptr<GPUDawnTogglesDescriptor>> {
  static std::shared_ptr<GPUDawnTogglesDescriptor>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    auto result = std::make_shared<GPUDawnTogglesDescriptor>();
    if (outOfBounds || arg.isNull() || arg.isUndefined()) return result;
    auto obj = arg.asObject(runtime);
    if (obj.hasProperty(runtime, "enable")) {
      result->enable = JSIConverter<std::vector<std::string>>::fromJSI(
          runtime, obj.getProperty(runtime, "enable"), false);
    }
    if (obj.hasProperty(runtime, "disable")) {
      result->disable = JSIConverter<std::vector<std::string>>::fromJSI(
          runtime, obj.getProperty(runtime, "disable"), false);
    }
    return result;
  }
  static jsi::Value toJSI(jsi::Runtime &,
                           std::shared_ptr<GPUDawnTogglesDescriptor>) {
    throw std::runtime_error("GPUDawnTogglesDescriptor: toJSI not supported");
  }
};

} // namespace rnwgpu
