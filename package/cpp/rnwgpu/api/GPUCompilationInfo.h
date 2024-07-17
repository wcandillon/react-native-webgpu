#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Convertors.h"
#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"
#include "Convertors.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUCompilationInfo : public m::HybridObject {
public:
  explicit GPUCompilationInfo(wgpu::CompilationInfo instance)
      : HybridObject("GPUCompilationInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCompilationInfo::getBrand, this);
  }

  inline const wgpu::CompilationInfo get() { return _instance; }

private:
  wgpu::CompilationInfo _instance;
};

bool conv(wgpu::CompilationInfo &out,
          const std::shared_ptr<GPUCompilationInfo> &in) {
  out = in->get();
  return true;
}

} // namespace rnwgpu