#pragma once

#include <string>

#include "Unions.h"

#include "RNFNativeObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;
namespace jsi = facebook::jsi;

class GPUCompilationMessage : public m::NativeObject<GPUCompilationMessage> {
public:
  static constexpr const char *CLASS_NAME = "GPUCompilationMessage";

  explicit GPUCompilationMessage(wgpu::CompilationMessage instance)
      : NativeObject(CLASS_NAME), _instance(instance) {}

public:
  std::string getBrand() { return CLASS_NAME; }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "__brand",
                  &GPUCompilationMessage::getBrand);
  }

  inline const wgpu::CompilationMessage get() { return _instance; }

private:
  wgpu::CompilationMessage _instance;
};

} // namespace rnwgpu
