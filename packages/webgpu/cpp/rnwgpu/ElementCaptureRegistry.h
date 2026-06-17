#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace rnwgpu {

// A single off-screen capture of a native view ("HTML in Canvas"), produced by
// the platform layer (Android: AHardwareBuffer via HardwareBufferRenderer; iOS:
// IOSurface via CARenderer, future) and consumed once by
// RNWebGPU.consumeCapturedElement(token).
struct CapturedElementEntry {
  // AHardwareBuffer* (Android) / IOSurfaceRef (iOS) already retained by the
  // producer. Ownership transfers to the JS consumer, which must call
  // RNWebGPU.releaseCapturedElement(handle) once the import has taken its own
  // reference.
  void *handle = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  // Producer GPU-completion fence as an OS file descriptor (-1 when the
  // producer already waited for completion on the CPU, so no wait fence is
  // needed). v1 waits on the CPU, so this is -1.
  int fenceFd = -1;
};

// Token-keyed hand-off between the platform capture (which runs asynchronously,
// off the JS thread) and the synchronous JS consumer. The token is a plain int
// so it crosses the React Native bridge without precision loss; the 64-bit
// handle comes back to JS as a BigInt.
class ElementCaptureRegistry {
public:
  static ElementCaptureRegistry &getInstance() {
    static ElementCaptureRegistry instance;
    return instance;
  }

  void store(int token, CapturedElementEntry entry) {
    std::lock_guard<std::mutex> lock(_mutex);
    _entries[token] = entry;
  }

  std::optional<CapturedElementEntry> consume(int token) {
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _entries.find(token);
    if (it == _entries.end()) {
      return std::nullopt;
    }
    auto entry = it->second;
    _entries.erase(it);
    return entry;
  }

private:
  std::mutex _mutex;
  std::unordered_map<int, CapturedElementEntry> _entries;
};

} // namespace rnwgpu
