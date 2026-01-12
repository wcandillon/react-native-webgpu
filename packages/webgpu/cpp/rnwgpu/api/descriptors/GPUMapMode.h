#pragma once
#include <string>

#include <RNFNativeObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUMapMode : public NativeObject<GPUMapMode> {
public:
  static constexpr const char *CLASS_NAME = "GPUMapMode";

  GPUMapMode() : NativeObject(CLASS_NAME) {}

public:
  double Read() { return static_cast<double>(wgpu::MapMode::Read); }
  double Write() { return static_cast<double>(wgpu::MapMode::Write); }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "READ", &GPUMapMode::Read);
    installGetter(runtime, prototype, "WRITE", &GPUMapMode::Write);
  }
};
} // namespace rnwgpu
