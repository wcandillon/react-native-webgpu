#pragma once

#include <memory>
#include <string>
#include <future>
#include <vector>

#include "Unions.h"
#include "Convertors.h"
#include <RNFHybridObject.h>

#include "AsyncRunner.h"
#include "ArrayBuffer.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUComputePipeline.h"
#include "GPUBindGroup.h"

namespace rnwgpu {

namespace m = margelo;

class GPUComputePassEncoder : public m::HybridObject {
public:
    explicit GPUComputePassEncoder(wgpu::ComputePassEncoder instance, std::string label) : HybridObject("GPUComputePassEncoder"), _instance(instance), _label(label) {}

public:
  std::string getBrand() { return _name; }


  void setPipeline(std::shared_ptr<GPUComputePipeline> pipeline);
void dispatchWorkgroups(uint32_t workgroupCountX, std::optional<uint32_t> workgroupCountY, std::optional<uint32_t> workgroupCountZ);
void end();
void setBindGroup(uint32_t groupIndex, std::shared_ptr<GPUBindGroup> group, std::optional<std::vector<uint32_t>> dynamicOffsets);

  

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePassEncoder::getBrand, this);
    registerHybridMethod("setPipeline", &GPUComputePassEncoder::setPipeline, this);
registerHybridMethod("dispatchWorkgroups", &GPUComputePassEncoder::dispatchWorkgroups, this);
registerHybridMethod("end", &GPUComputePassEncoder::end, this);
registerHybridMethod("setBindGroup", &GPUComputePassEncoder::setBindGroup, this);
    
    registerHybridGetter("label", &GPUComputePassEncoder::getLabel, this);
  }
  
  inline const wgpu::ComputePassEncoder get() {
    return _instance;
  }

 private:
  wgpu::ComputePassEncoder _instance;
std::string _label;
  
};
} // namespace rnwgpu