#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPUBuffer.h"
#include "GPUComputePipeline.h"

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
  void dispatchWorkgroups(uint32_t workgroupCountX,
                          std::optional<uint32_t> workgroupCountY,
                          std::optional<uint32_t> workgroupCountZ);
  void dispatchWorkgroupsIndirect(std::shared_ptr<GPUBuffer> indirectBuffer,
                                  uint64_t indirectOffset);
  void end();
  void pushDebugGroup(std::string groupLabel);
  void popDebugGroup();
  void insertDebugMarker(std::string markerLabel);
  void setBindGroup(
      uint32_t index,
      std::variant<std::nullptr_t, std::shared_ptr<GPUBindGroup>> bindGroup,
      std::optional<std::vector<uint32_t>> dynamicOffsets);

  std::string getLabel() { return _label; }
  void setLabel(const std::string &label) {
    _label = label;
    _instance.SetLabel(_label.c_str());
  }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUComputePassEncoder::getBrand, this);
    registerHybridMethod("setPipeline", &GPUComputePassEncoder::setPipeline,
                         this);
    registerHybridMethod("dispatchWorkgroups",
                         &GPUComputePassEncoder::dispatchWorkgroups, this);
    registerHybridMethod("dispatchWorkgroupsIndirect",
                         &GPUComputePassEncoder::dispatchWorkgroupsIndirect,
                         this);
    registerHybridMethod("end", &GPUComputePassEncoder::end, this);
    registerHybridMethod("pushDebugGroup",
                         &GPUComputePassEncoder::pushDebugGroup, this);
    registerHybridMethod("popDebugGroup", &GPUComputePassEncoder::popDebugGroup,
                         this);
    registerHybridMethod("insertDebugMarker",
                         &GPUComputePassEncoder::insertDebugMarker, this);
    registerHybridMethod("setBindGroup", &GPUComputePassEncoder::setBindGroup,
                         this);

    registerHybridGetter("label", &GPUComputePassEncoder::getLabel, this);
    registerHybridSetter("label", &GPUComputePassEncoder::setLabel, this);
  }

  inline const wgpu::ComputePassEncoder get() { return _instance; }

private:
  wgpu::ComputePassEncoder _instance;
  std::string _label;
};

} // namespace rnwgpu