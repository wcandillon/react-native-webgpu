#pragma once

#include <memory>

#include "GPU.h"
#include "PlatformContext.h"
#include "SurfaceRegistry.h"

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
  RNWebGPUManager(jsi::Runtime *jsRuntime,
                  std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker,
                  std::shared_ptr<PlatformContext> platformContext);
  ~RNWebGPUManager();

  /**
   * Installs globalThis.__webgpuWorkletHelpers, the HostObject the worklet
   * serializer (registerWebGPUForReanimated) captures to detect and box
   * WebGPU objects. Only needed on the main runtime: the serializer worklets
   * carry it to every other runtime by reference. See WebGPUWorkletHelpers.
   */
  static void installWebGPUWorkletHelpers(jsi::Runtime &runtime);

  /**
   * Applies a surface attach latched by the platform UI thread (see
   * SurfaceInfo::applyPendingAttach) from the JS thread. Surface attaches are
   * normally adopted at the next frame boundary by whichever thread renders;
   * this flush covers contexts that are not actively rendering (static
   * content), so the last offscreen frame still makes it on screen.
   */
  void flushPendingSurfaceTransition(std::shared_ptr<SurfaceInfo> info);

private:
  jsi::Runtime *_jsRuntime;
  std::shared_ptr<facebook::react::CallInvoker> _jsCallInvoker;

public:
  wgpu::Instance _gpu;
  std::shared_ptr<PlatformContext> _platformContext;
};

} // namespace rnwgpu
