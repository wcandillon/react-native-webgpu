#pragma once
#include <string>

#include <RNFNativeObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace jsi = facebook::jsi;

class GPUColorWrite : public NativeObject<GPUColorWrite> {
public:
  static constexpr const char *CLASS_NAME = "GPUColorWrite";

  GPUColorWrite() : NativeObject(CLASS_NAME) {}

public:
  double Red() { return static_cast<double>(wgpu::ColorWriteMask::Red); }
  double Green() { return static_cast<double>(wgpu::ColorWriteMask::Green); }
  double Blue() { return static_cast<double>(wgpu::ColorWriteMask::Blue); }
  double Alpha() { return static_cast<double>(wgpu::ColorWriteMask::Alpha); }
  double All() { return static_cast<double>(wgpu::ColorWriteMask::All); }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "RED", &GPUColorWrite::Red);
    installGetter(runtime, prototype, "GREEN", &GPUColorWrite::Green);
    installGetter(runtime, prototype, "BLUE", &GPUColorWrite::Blue);
    installGetter(runtime, prototype, "ALPHA", &GPUColorWrite::Alpha);
    installGetter(runtime, prototype, "ALL", &GPUColorWrite::All);
  }
};
} // namespace rnwgpu
