#pragma once

#include "webgpu_cpp.h"
#include <optional>
#include <string>

namespace rnwgpu {

struct GPURenderPipelineDescriptor {
  unknown vertex;                      /* GPUVertexState */
  std::optional<unknown> primitive;    /* GPUPrimitiveState */
  std::optional<unknown> depthStencil; /* GPUDepthStencilState */
  std::optional<unknown> multisample;  /* GPUMultisampleState */
  std::optional<unknown> fragment;     /* GPUFragmentState */
  unknown layout;                      /* | GPUPipelineLayout
                             | GPUAutoLayoutMode */
  std::optional<std::string> label;    /* string */
};

} // namespace rnwgpu