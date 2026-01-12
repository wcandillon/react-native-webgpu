#pragma once
#include <string>

#include <RNFNativeObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;
namespace jsi = facebook::jsi;

class GPUShaderStage : public m::NativeObject<GPUShaderStage> {
public:
  static constexpr const char *CLASS_NAME = "GPUShaderStage";

  GPUShaderStage() : NativeObject(CLASS_NAME) {}

public:
  double Vertex() { return static_cast<double>(wgpu::ShaderStage::Vertex); }
  double Fragment() { return static_cast<double>(wgpu::ShaderStage::Fragment); }
  double Compute() { return static_cast<double>(wgpu::ShaderStage::Compute); }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "VERTEX", &GPUShaderStage::Vertex);
    installGetter(runtime, prototype, "FRAGMENT", &GPUShaderStage::Fragment);
    installGetter(runtime, prototype, "COMPUTE", &GPUShaderStage::Compute);
  }
};
} // namespace rnwgpu
