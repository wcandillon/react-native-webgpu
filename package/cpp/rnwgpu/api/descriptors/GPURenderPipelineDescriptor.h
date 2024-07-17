#pragma once

#include <optional>
#include <string>
#include <variant>

#include "webgpu/webgpu_cpp.h"

#include "GPUDepthStencilState.h"
#include "GPUFragmentState.h"
#include "GPUMultisampleState.h"
#include "GPUPipelineLayout.h"
#include "GPUPrimitiveState.h"
#include "GPUVertexState.h"

namespace rnwgpu {

struct GPURenderPipelineDescriptor {
  std::shared_ptr<GPUVertexState> vertex; /* GPUVertexState */
  std::optional<std::shared_ptr<GPUPrimitiveState>>
      primitive; /* GPUPrimitiveState */
  std::optional<std::shared_ptr<GPUDepthStencilState>>
      depthStencil; /* GPUDepthStencilState */
  std::optional<std::shared_ptr<GPUMultisampleState>>
      multisample; /* GPUMultisampleState */
  std::optional<std::shared_ptr<GPUFragmentState>>
      fragment; /* GPUFragmentState */
  std::variant<std::shared_ptr<GPUPipelineLayout>, unknown> layout; /* |
        GPUPipelineLayout | GPUAutoLayoutMode */
  std::optional<std::string> label;                                 /* string */
};

} // namespace rnwgpu