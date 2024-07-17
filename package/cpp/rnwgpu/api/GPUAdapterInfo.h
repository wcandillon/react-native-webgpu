#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "RNFHybridObject.h"
#include "Unions.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

class GPUAdapterInfo : public m::HybridObject {
public:
  explicit GPUAdapterInfo(wgpu::AdapterInfo instance)
      : HybridObject("GPUAdapterInfo"), _instance(instance) {}

public:
  std::string getBrand() { return _name; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUAdapterInfo::getBrand, this);
  }

  inline const wgpu::AdapterInfo get() { return _instance; }

private:
  wgpu::AdapterInfo _instance;
};

// bool conv(wgpu::AdapterInfo &out, const std::shared_ptr<GPUAdapterInfo> &in)
// {
//   out = in->get();
//   return true;
// }

} // namespace rnwgpu