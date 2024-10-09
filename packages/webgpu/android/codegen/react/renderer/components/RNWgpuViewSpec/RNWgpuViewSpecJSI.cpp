#include "RNWgpuViewSpecJSI.h"

namespace facebook::react {

static jsi::Value __hostFunction_NativeWebGPUModuleCxxSpecJSI_install(jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value* args, size_t count) {
  return static_cast<NativeWebGPUModuleCxxSpecJSI *>(&turboModule)->install(
    rt
  );
}
static jsi::Value __hostFunction_NativeWebGPUModuleCxxSpecJSI_createSurfaceContext(jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value* args, size_t count) {
  return static_cast<NativeWebGPUModuleCxxSpecJSI *>(&turboModule)->createSurfaceContext(
    rt,
    args[0].asNumber()
  );
}

NativeWebGPUModuleCxxSpecJSI::NativeWebGPUModuleCxxSpecJSI(std::shared_ptr<CallInvoker> jsInvoker)
  : TurboModule("WebGPUModule", jsInvoker) {
  methodMap_["install"] = MethodMetadata {0, __hostFunction_NativeWebGPUModuleCxxSpecJSI_install};
  methodMap_["createSurfaceContext"] = MethodMetadata {1, __hostFunction_NativeWebGPUModuleCxxSpecJSI_createSurfaceContext};
}


} // namespace facebook::react
