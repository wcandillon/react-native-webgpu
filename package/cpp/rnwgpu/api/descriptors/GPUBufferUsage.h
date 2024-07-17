#pragma once
#include <string>

#include <RNFHybridObject.h>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBufferUsage : public m::HybridObject {
public:
  GPUBufferUsage() : HybridObject("GPUBufferUsage") {}

public:
  double MapRead() {
      return static_cast<double>(wgpu::BufferUsage::MapRead);
    }
    double MapWrite() {
      return static_cast<double>(wgpu::BufferUsage::MapWrite);
    }
    double CopySrc() {
      return static_cast<double>(wgpu::BufferUsage::CopySrc);
    }
    double CopyDst() {
      return static_cast<double>(wgpu::BufferUsage::CopyDst);
    }
    double Index() {
      return static_cast<double>(wgpu::BufferUsage::Index);
    }
    double Vertex() {
      return static_cast<double>(wgpu::BufferUsage::Vertex);
    }
    double Uniform() {
      return static_cast<double>(wgpu::BufferUsage::Uniform);
    }
    double Storage() {
      return static_cast<double>(wgpu::BufferUsage::Storage);
    }
    double Indirect() {
      return static_cast<double>(wgpu::BufferUsage::Indirect);
    }
    double QueryResolve() {
      return static_cast<double>(wgpu::BufferUsage::QueryResolve);
    }

  void loadHybridMethods() override {
    registerHybridGetter("MAP_READ", &GPUBufferUsage::MapRead, this);
    registerHybridGetter("MAP_WRITE", &GPUBufferUsage::MapWrite, this);
    registerHybridGetter("COPY_SRC", &GPUBufferUsage::CopySrc, this);
    registerHybridGetter("COPY_DST", &GPUBufferUsage::CopyDst, this);
    registerHybridGetter("INDEX", &GPUBufferUsage::Index, this);
    registerHybridGetter("VERTEX", &GPUBufferUsage::Vertex, this);
    registerHybridGetter("UNIFORM", &GPUBufferUsage::Uniform, this);
    registerHybridGetter("STORAGE", &GPUBufferUsage::Storage, this);
    registerHybridGetter("INDIRECT", &GPUBufferUsage::Indirect, this);
    registerHybridGetter("QUERY_RESOLVE", &GPUBufferUsage::QueryResolve, this);
  }
};
} // namespace rnwgpu