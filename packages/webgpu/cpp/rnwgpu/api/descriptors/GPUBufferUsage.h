#pragma once
#include <string>

#include <RNFNativeObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;
namespace jsi = facebook::jsi;

class GPUBufferUsage : public m::NativeObject<GPUBufferUsage> {
public:
  static constexpr const char *CLASS_NAME = "GPUBufferUsage";

  GPUBufferUsage() : NativeObject(CLASS_NAME) {}

public:
  double MapRead() { return static_cast<double>(wgpu::BufferUsage::MapRead); }
  double MapWrite() { return static_cast<double>(wgpu::BufferUsage::MapWrite); }
  double CopySrc() { return static_cast<double>(wgpu::BufferUsage::CopySrc); }
  double CopyDst() { return static_cast<double>(wgpu::BufferUsage::CopyDst); }
  double Index() { return static_cast<double>(wgpu::BufferUsage::Index); }
  double Vertex() { return static_cast<double>(wgpu::BufferUsage::Vertex); }
  double Uniform() { return static_cast<double>(wgpu::BufferUsage::Uniform); }
  double Storage() { return static_cast<double>(wgpu::BufferUsage::Storage); }
  double Indirect() { return static_cast<double>(wgpu::BufferUsage::Indirect); }
  double QueryResolve() {
    return static_cast<double>(wgpu::BufferUsage::QueryResolve);
  }

  static void definePrototype(jsi::Runtime &runtime, jsi::Object &prototype) {
    installGetter(runtime, prototype, "MAP_READ", &GPUBufferUsage::MapRead);
    installGetter(runtime, prototype, "MAP_WRITE", &GPUBufferUsage::MapWrite);
    installGetter(runtime, prototype, "COPY_SRC", &GPUBufferUsage::CopySrc);
    installGetter(runtime, prototype, "COPY_DST", &GPUBufferUsage::CopyDst);
    installGetter(runtime, prototype, "INDEX", &GPUBufferUsage::Index);
    installGetter(runtime, prototype, "VERTEX", &GPUBufferUsage::Vertex);
    installGetter(runtime, prototype, "UNIFORM", &GPUBufferUsage::Uniform);
    installGetter(runtime, prototype, "STORAGE", &GPUBufferUsage::Storage);
    installGetter(runtime, prototype, "INDIRECT", &GPUBufferUsage::Indirect);
    installGetter(runtime, prototype, "QUERY_RESOLVE",
                  &GPUBufferUsage::QueryResolve);
  }
};
} // namespace rnwgpu
