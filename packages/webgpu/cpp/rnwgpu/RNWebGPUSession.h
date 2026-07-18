#pragma once

#include <atomic>
#include <cstdint>

namespace rnwgpu {

using RNWebGPUSessionId = std::uint64_t;

inline constexpr RNWebGPUSessionId kInvalidRNWebGPUSessionId = 0;
// Session ids cross the Fabric boundary as a JavaScript number/Codegen Double.
// Keeping them in the safe-integer range preserves their identity exactly on
// every supported platform.
inline constexpr RNWebGPUSessionId kMaxRNWebGPUSessionId =
    (RNWebGPUSessionId{1} << 53U) - 1U;

/**
 * Process-independent lifetime token shared by objects created for one
 * React Native JavaScript runtime.
 */
class RNWebGPUSessionState final {
public:
  explicit RNWebGPUSessionState(RNWebGPUSessionId id) noexcept : _id(id) {}

  RNWebGPUSessionId id() const noexcept { return _id; }

  bool isActive() const noexcept {
    return _active.load(std::memory_order_acquire);
  }

  void invalidate() noexcept {
    _active.store(false, std::memory_order_release);
  }

private:
  RNWebGPUSessionId _id;
  std::atomic<bool> _active{true};
};

} // namespace rnwgpu
