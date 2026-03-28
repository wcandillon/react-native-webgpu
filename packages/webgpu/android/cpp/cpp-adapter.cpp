#include <memory>
#include <unordered_map>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <ReactCommon/CallInvokerHolder.h>
#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "AndroidSurfaceBridge.h"
#include "GPUCanvasContext.h"
#include "RNWebGPUManager.h"
#include "WGPULogger.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;

// Helper to get the AndroidSurfaceBridge for a given contextId.
static std::shared_ptr<rnwgpu::AndroidSurfaceBridge> getAndroidBridge(int contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  return std::static_pointer_cast<rnwgpu::AndroidSurfaceBridge>(
      registry.getSurfaceInfo(contextId));
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject /* this */, jlong jsRuntime,
    jobject jsCallInvokerHolder, jobject blobModule) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  jobject globalBlobModule = env->NewGlobalRef(blobModule);
  auto jsCallInvoker{
      facebook::jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>{
          reinterpret_cast<facebook::react::CallInvokerHolder::javaobject>(
              jsCallInvokerHolder)} -> cthis()
          ->getCallInvoker()};
  auto platformContext =
      std::make_shared<rnwgpu::AndroidPlatformContext>(globalBlobModule);
  manager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, jsCallInvoker,
                                                      platformContext);
}

static wgpu::Surface makeSurface(wgpu::Instance instance, void *window) {
  wgpu::SurfaceSourceAndroidNativeWindow androidSurfaceDesc;
  androidSurfaceDesc.window = reinterpret_cast<ANativeWindow *>(window);
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &androidSurfaceDesc;
  return instance.CreateSurface(&surfaceDescriptor);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject jSurface, jint contextId, jfloat width,
    jfloat height) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();

  auto bridge = std::static_pointer_cast<rnwgpu::AndroidSurfaceBridge>(
    registry.getSurfaceInfoOrCreate(contextId, manager->_gpu));
  if (bridge) {
    auto old = bridge->switchToOffscreen();
    if (old) ANativeWindow_release(old);
  }

  // It runs ANativeWindow_acquire() internally
  auto window = ANativeWindow_fromSurface(env, jSurface);
  auto surface = makeSurface(manager->_gpu.gpu, window);
  bridge->switchToOnscreen(window, surface);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceChanged(
    JNIEnv *env, jobject thiz, jobject jSurface, jint contextId, jfloat width,
    jfloat height) {
  // No-op for now
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_switchToOffscreenSurface(
  JNIEnv *env, jobject thiz, jint contextId) {

  auto bridge = getAndroidBridge(contextId);
  if (bridge) {
    auto *window = bridge->switchToOffscreen();
    ANativeWindow_release(window);
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onViewDetached(
    JNIEnv *env, jobject thiz, jint contextId) {
  // Called from onDropViewInstance when the React component is permanently unmounted.
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.removeSurfaceInfo(contextId);
}
