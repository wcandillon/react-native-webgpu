#pragma once

#include <memory>
#include <string>
#include <utility>

#include "webgpu/webgpu_cpp.h"

#include "EnumMapper.h"
#include "JSIConverter.h"

namespace rnwgpu {

class GPUError {

public:
  GPUError(wgpu::ErrorType aType, std::string aMessage)
      : type(aType), message(std::move(aMessage)) {}

  wgpu::ErrorType type;
  std::string message;
};

template <> struct JSIConverter<std::shared_ptr<GPUError>> {
  static std::shared_ptr<GPUBindGroupEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    throw std::runtime_error("Invalid GPUBindGroupEntry::fromJSI()");
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<GPUError> arg) {
    jsi::Object result(runtime);
    result.setProperty(
        runtime, "message",
        jsi::String::createFromUtf8(runtime, arg->message.c_str()));
    return result;
  }
};

} // namespace rnwgpu
