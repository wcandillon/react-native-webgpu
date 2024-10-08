#include <memory>
#include <unordered_map>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "GPUCanvasContext.h"
#include "RNWebGPUManager.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject /* this */, jlong jsRuntime, jobject jsInvokerHolder,
    jobject blobModule) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  jobject globalBlobModule = env->NewGlobalRef(blobModule);
  auto platformContext =
      std::make_shared<rnwgpu::AndroidPlatformContext>(globalBlobModule);
  manager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, nullptr,
                                                      platformContext);
}

// TODO: remove
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_createSurfaceContext(JNIEnv *env, jobject thiz,
                                                  jlong jsRuntime,
                                                  jint contextId) {}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceChanged(
    JNIEnv *env, jobject thiz, jobject surface, jint contextId, jfloat width,
    jfloat height) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.updateSurface(contextId, width, height);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject surface, jint contextId, jfloat width,
    jfloat height) {
  auto window = ANativeWindow_fromSurface(env, surface);
  // ANativeWindow_acquire(window);
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.addSurface(contextId, window, width, height, nullptr);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceDestroy(
    JNIEnv *env, jobject thiz, jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto canvas = registry.getSurface(contextId);
  ANativeWindow_release(reinterpret_cast<ANativeWindow *>(canvas->surface));
  registry.removeSurface(contextId);
}
