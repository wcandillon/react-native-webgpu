#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "GPU.h"
#include "PlatformContext.h"
#include "RNWebGPUSession.h"

namespace facebook {
namespace jsi {
class Runtime;
} // namespace jsi
namespace react {
class CallInvoker;
}
} // namespace facebook

namespace rnwgpu {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

class RNWebGPUManager {
public:
  RNWebGPUManager(RNWebGPUSessionId sessionId, jsi::Runtime *jsRuntime,
                  std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
                  std::shared_ptr<PlatformContext> platformContext);
  ~RNWebGPUManager();

  RNWebGPUManager(const RNWebGPUManager &) = delete;
  RNWebGPUManager &operator=(const RNWebGPUManager &) = delete;
  RNWebGPUManager(RNWebGPUManager &&) = delete;
  RNWebGPUManager &operator=(RNWebGPUManager &&) = delete;

  RNWebGPUSessionId sessionId() const noexcept { return _sessionState->id(); }
  bool isActive() const noexcept { return _sessionState->isActive(); }
  void invalidate() noexcept;

  static RNWebGPUSessionId sessionForRuntime(jsi::Runtime &runtime);

  /**
   * Install global helper functions for Worklets serialization.
   * This installs __webgpuIsWebGPUObject and __webgpuBox on the global object.
   * Can be called on any runtime (main JS, UI, or custom worklet runtimes).
   */
  static void installWebGPUWorkletHelpers(jsi::Runtime &runtime);

private:
  jsi::Runtime *_jsRuntime;
  std::shared_ptr<facebook::react::CallInvoker> _jsCallInvoker;
  std::shared_ptr<RNWebGPUSessionState> _sessionState;

public:
  wgpu::Instance _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
};

struct RNWebGPUManagerSnapshot {
  RNWebGPUSessionId sessionId{kInvalidRNWebGPUSessionId};
  std::shared_ptr<RNWebGPUManager> manager;

  explicit operator bool() const noexcept {
    return sessionId != kInvalidRNWebGPUSessionId && manager != nullptr;
  }
};

/**
 * Synchronizes process-wide publication of the manager used by native views.
 * Managers themselves remain owned by their module/runtime session.
 */
class RNWebGPUManagerRegistry final {
public:
  static RNWebGPUManagerRegistry &getInstance();

  RNWebGPUSessionId createSession();
  void publish(RNWebGPUSessionId sessionId,
               std::shared_ptr<RNWebGPUManager> manager);
  RNWebGPUManagerSnapshot acquire(RNWebGPUSessionId sessionId);
  RNWebGPUManagerSnapshot getActive() const;
  RNWebGPUManagerSnapshot get(RNWebGPUSessionId sessionId) const;

  /**
   * Releases one module owner. The session is closed when its last owner
   * leaves; a stale release never changes a newer active session.
   */
  std::shared_ptr<RNWebGPUManager> release(RNWebGPUSessionId sessionId);

private:
  struct Entry {
    std::shared_ptr<RNWebGPUManager> manager;
    std::size_t owners{0};
  };

  RNWebGPUManagerRegistry() = default;

  mutable std::mutex _mutex;
  std::unordered_map<RNWebGPUSessionId, Entry> _entries;
  RNWebGPUSessionId _activeSessionId{kInvalidRNWebGPUSessionId};
  RNWebGPUSessionId _nextSessionId{1};
};

} // namespace rnwgpu
