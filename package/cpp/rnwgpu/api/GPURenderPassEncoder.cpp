#include "GPURenderPassEncoder.h"

namespace rnwgpu {

void GPURenderPassEncoder::end() { _instance.End(); }

void GPURenderPassEncoder::setPipeline(
    std::shared_ptr<GPURenderPipeline> pipeline) {
  _instance.SetPipeline(pipeline->get());
}

void GPURenderPassEncoder::draw(uint32_t vertexCount,
                                std::optional<uint32_t> instanceCount,
                                std::optional<uint32_t> firstVertex,
                                std::optional<uint32_t> firstInstance) {
  _instance.Draw(vertexCount, instanceCount.value_or(1),
                 firstVertex.value_or(0), firstInstance.value_or(0));
}

} // namespace rnwgpu