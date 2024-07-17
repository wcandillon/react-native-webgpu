#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPURenderPassDescriptor {
  unknown colorAttachments; // Iterable<GPURenderPassColorAttachment | null>
  std::optional<unknown>
      depthStencilAttachment; // GPURenderPassDepthStencilAttachment
  std::optional<unknown> occlusionQuerySet; // GPUQuerySet
  std::optional<unknown> timestampWrites;   // GPURenderPassTimestampWrites
  std::optional<double> maxDrawCount;       // GPUSize64
  std::optional<std::string> label;         // string
};

} // namespace rnwgpu