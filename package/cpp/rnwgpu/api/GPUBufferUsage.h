#pragma once

#include <RNFHybridObject.h>

#include "webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUBufferUsage : public m::HybridObject {
public:
  GPUBufferUsage() : HybridObject("GPUBufferUsage") {}

public:
  wgpu::BufferUsage MapRead() { return wgpu::BufferUsage::MapRead; };
  wgpu::BufferUsage MapWrite() { return wgpu::BufferUsage::MapWrite; };
  wgpu::BufferUsage CopySrc() { return wgpu::BufferUsage::CopySrc; };
  wgpu::BufferUsage CopyDst() { return wgpu::BufferUsage::CopyDst; };
  wgpu::BufferUsage Index() { return wgpu::BufferUsage::Index; };
  wgpu::BufferUsage Vertex() { return wgpu::BufferUsage::Vertex; };
  wgpu::BufferUsage Uniform() { return wgpu::BufferUsage::Uniform; };
  wgpu::BufferUsage Storage() { return wgpu::BufferUsage::Storage; };
  wgpu::BufferUsage Indirect() { return wgpu::BufferUsage::Indirect; };
  wgpu::BufferUsage QueryResolve() { return wgpu::BufferUsage::QueryResolve; }

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