#pragma once

#include <variant>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "ArrayBuffer.h"
#include "AsyncRunner.h"

#include "webgpu/webgpu_cpp.h"

#include "GPUBindGroup.h"
#include "GPURenderPipeline.h"

namespace rnwgpu {

namespace m = margelo;

class GPURenderPassEncoder : public m::HybridObject {
public:
  explicit GPURenderPassEncoder(wgpu::RenderPassEncoder instance,
                                std::string label)
      : HybridObject("GPURenderPassEncoder"), _instance(instance),
        _label(label) {}

public:
  std::string getBrand() { return _name; }

  void end();
  void setBindGroup(uint32_t groupIndex, std::shared_ptr<GPUBindGroup> group,
                    std::optional<std::vector<uint32_t>> dynamicOffsets);
  void setPipeline(std::shared_ptr<GPURenderPipeline> pipeline);
  void draw(uint32_t vertexCount, std::optional<uint32_t> instanceCount,
            std::optional<uint32_t> firstVertex,
            std::optional<uint32_t> firstInstance);

  std::string getLabel() { return _label; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPURenderPassEncoder::getBrand, this);
    registerHybridMethod("end", &GPURenderPassEncoder::end, this);
    registerHybridMethod("setBindGroup", &GPURenderPassEncoder::setBindGroup,
                         this);
    registerHybridMethod("setPipeline", &GPURenderPassEncoder::setPipeline,
                         this);
    registerHybridMethod("draw", &GPURenderPassEncoder::draw, this);

    registerHybridGetter("label", &GPURenderPassEncoder::getLabel, this);
  }

  inline const wgpu::RenderPassEncoder get() { return _instance; }

private:
  wgpu::RenderPassEncoder _instance;
  std::string _label;
};

} // namespace rnwgpu
