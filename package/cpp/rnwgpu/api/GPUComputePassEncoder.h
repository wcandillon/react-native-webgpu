#pragma once

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPUComputePipeline.h"
#include "variant.h"
#include "vector.h"

namespace rnwgpu {

namespace m = margelo;

class GPUComputePassEncoder : public m::HybridObject {
public:
  explicit GPUComputePassEncoder(wgpu::ComputePassEncoder instance,
                                 std::string label)
      : HybridObject("GPUComputePassEncoder"), _instance(instance),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  void setPipeline(std::shared_ptr<GPUComputePipeline> pipeline);
  void dispatchWorkgroups(double workgroupCountX,
                          std::optional<double> workgroupCountY,
                          std::optional<double> workgroupCountZ);
  void end();
  void setBindGroup(
      double index,
      std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>> bindGroup,
      std::optional<std::vector<double>> dynamicOffsets);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePassEncoder::getBrand, this);
    registerHybridMethod("setPipeline", &GPUComputePassEncoder::setPipeline,
                         this);
    registerHybridMethod("dispatchWorkgroups",
                         &GPUComputePassEncoder::dispatchWorkgroups, this);
    registerHybridMethod("end", &GPUComputePassEncoder::end, this);
    registerHybridMethod("setBindGroup", &GPUComputePassEncoder::setBindGroup,
                         this);

    registerHybridGetter("label", &GPUComputePassEncoder::getLabel, this);
  }

  inline const wgpu::ComputePassEncoder get() { return _instance; }

private:
  wgpu::ComputePassEncoder _instance;
  std::string _label;
};

} // namespace rnwgpu