#pragma once

#include <optional>

namespace rnwgpu {

struct GPUSamplerBindingLayout {
  std::optional<unknown> type; // GPUSamplerBindingType
};

} // namespace rnwgpu