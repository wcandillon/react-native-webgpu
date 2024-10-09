#pragma once

#include <ReactCommon/JavaTurboModule.h>
#include <ReactCommon/TurboModule.h>
#include <jsi/jsi.h>

#include <react/renderer/components/RNWgpuViewSpec/ComponentDescriptors.h>

namespace facebook::react {

/**
 * JNI C++ class for module 'NativeWebGPUModule'
 */
class JSI_EXPORT NativeWebGPUModuleSpecJSI : public JavaTurboModule {
public:
  NativeWebGPUModuleSpecJSI(const JavaTurboModule::InitParams &params);
};


JSI_EXPORT
std::shared_ptr<TurboModule> RNWgpuViewSpec_ModuleProvider(const std::string &moduleName, const JavaTurboModule::InitParams &params);

} // namespace facebook::react
