#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

// Umbrella feature name owned by react-native-wgpu. Backed by a
// platform-specific pair of Dawn features. The prefix is intentional: this
// string is not part of the WebGPU spec, it is our API surface for the
// "import a native surface as a sampleable texture" capability.
inline constexpr const char *kRnSharedTextureMemoryFeature =
    "rnwebgpu/shared-texture-memory";

// Dawn features that back the umbrella on the current platform. Empty on
// platforms where the capability is not available, in which case the umbrella
// behaves as a no-op (it won't appear in adapter.features and asking for it
// in requiredFeatures expands to nothing).
inline std::vector<wgpu::FeatureName> rnSharedTextureMemoryBackingFeatures() {
#if defined(__APPLE__)
  return {wgpu::FeatureName::SharedTextureMemoryIOSurface,
          wgpu::FeatureName::SharedFenceMTLSharedEvent};
#elif defined(__ANDROID__)
  return {wgpu::FeatureName::SharedTextureMemoryAHardwareBuffer,
          wgpu::FeatureName::SharedFenceSyncFD};
#else
  return {};
#endif
}

// If every Dawn feature backing the umbrella is in `enabled`, add the
// umbrella name to `out`. Used by adapter.features / device.features so JS
// callers can see (and call .has on) the same name they pass in.
inline void
maybeSynthesizeRnSharedTextureMemoryFeature(
    const std::unordered_set<wgpu::FeatureName> &enabled,
    std::unordered_set<std::string> &out) {
  auto backing = rnSharedTextureMemoryBackingFeatures();
  if (backing.empty()) {
    return;
  }
  for (auto f : backing) {
    if (enabled.find(f) == enabled.end()) {
      return;
    }
  }
  out.insert(kRnSharedTextureMemoryFeature);
}

} // namespace rnwgpu
