#pragma once

#include <optional>
#include <string>
#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "GPUQuerySet.h"
#include "GPURenderPassColorAttachment.h"
#include "GPURenderPassDepthStencilAttachment.h"
#include "GPURenderPassTimestampWrites.h"

namespace rnwgpu {

struct GPURenderPassDescriptor {
  std::vector<std::variant<std::nullptr_t,
                           std::shared_ptr<GPURenderPassColorAttachment>>>
      colorAttachments; /* Iterable<GPURenderPassColorAttachment | null> */
  std::optional<std::shared_ptr<GPURenderPassDepthStencilAttachment>>
      depthStencilAttachment; /* GPURenderPassDepthStencilAttachment */
  std::optional<std::shared_ptr<GPUQuerySet>>
      occlusionQuerySet; /* GPUQuerySet */
  std::optional<std::shared_ptr<GPURenderPassTimestampWrites>>
      timestampWrites;                /* GPURenderPassTimestampWrites */
  std::optional<double> maxDrawCount; /* GPUSize64 */
  std::optional<std::string> label;   /* string */
};

} // namespace rnwgpu