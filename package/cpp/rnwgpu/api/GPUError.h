#pragma once

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"
#include "RNFEnumMapper.h"

namespace rnwgpu {

struct GPUError {
  wgpu::ErrorType type;
  char const *message;
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
    result.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, arg->message));
    return result;
  }
};

} // namespace margelo
