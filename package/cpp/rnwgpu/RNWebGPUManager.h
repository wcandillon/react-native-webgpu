#pragma once

#include <memory>
#include "GPU.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>

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
                  std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker);
  ~RNWebGPUManager();
  std::shared_ptr<GPU> gpu;
  static std::shared_ptr<wgpu::Surface> surface;
private:
  jsi::Runtime *_jsRuntime;
  std::shared_ptr<facebook::react::CallInvoker> _jsCallInvoker;
};

} // namespace rnwgpu
