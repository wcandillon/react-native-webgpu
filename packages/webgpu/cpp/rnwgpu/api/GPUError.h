#pragma once

#include <memory>
#include <string>

#include "webgpu/webgpu_cpp.h"

#include "RNFEnumMapper.h"
#include "RNFJSIConverter.h"

namespace rnwgpu {

class GPUError {

public:
  GPUError(wgpu::ErrorType aType, char const *aMessage)
      : type(aType), message(aMessage ? aMessage : "") {}

  wgpu::ErrorType type;
  std::string message;
};

} // namespace rnwgpu

namespace margelo {

using namespace rnwgpu; // NOLINT(build/namespaces)

template <> struct JSIConverter<std::shared_ptr<rnwgpu::GPUError>> {
  static std::shared_ptr<rnwgpu::GPUBindGroupEntry>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    throw std::runtime_error("Invalid GPUBindGroupEntry::fromJSI()");
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::shared_ptr<rnwgpu::GPUError> arg) {
    jsi::Object result(runtime);
    result.setProperty(
        runtime, "message",
        jsi::String::createFromUtf8(runtime, arg->message.c_str()));
    return result;
  }
};

} // namespace margelo
