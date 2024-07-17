#pragma once

#include <optional>
#include <string>

namespace rnwgpu {

struct GPURenderBundleEncoderDescriptor {
  std::optional<bool> depthReadOnly;   // boolean
  std::optional<bool> stencilReadOnly; // boolean
  unknown colorFormats;                // Iterable<GPUTextureFormat | null>
  std::optional<unknown> depthStencilFormat; // GPUTextureFormat
  std::optional<double> sampleCount;         // GPUSize32
  std::optional<std::string> label;          // string
};

} // namespace rnwgpu