#pragma once

#include <memory>
#include <string>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroupLayout.h"

namespace rnwgpu {

namespace m = margelo;

class GPUComputePipeline : public m::HybridObject {
public:
  explicit GPUComputePipeline(wgpu::ComputePipeline instance, std::string label)
      : HybridObject("GPUComputePipeline"), _instance(std::move(instance)),
        _label(std::move(label)) {}

public:
  std::string getBrand() { return _name; }

  std::shared_ptr<GPUBindGroupLayout> getBindGroupLayout(uint32_t index);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePipeline::getBrand, this);
    registerHybridMethod("getBindGroupLayout",
                         &GPUComputePipeline::getBindGroupLayout, this);

    registerHybridGetter("label", &GPUComputePipeline::getLabel, this);
  }

  inline const wgpu::ComputePipeline get() { return _instance; }

private:
  wgpu::ComputePipeline _instance;
  std::string _label;
};

} // namespace rnwgpu