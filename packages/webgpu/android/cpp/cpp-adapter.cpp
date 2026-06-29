#include <memory>
#include <unordered_map>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <ReactCommon/CallInvokerHolder.h>
#include <android/hardware_buffer_jni.h>
#include <android/native_window_jni.h>
#include <vector>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "GPUCanvasContext.h"
#include "RNWebGPUManager.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;

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

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceChanged(
    JNIEnv *env, jobject thiz, jobject surface, jint contextId, jfloat width,
    jfloat height) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.getSurfaceInfo(contextId)->resize(static_cast<int>(width),
                                             static_cast<int>(height));
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject jSurface, jint contextId, jfloat width,
    jfloat height) {
  auto window = ANativeWindow_fromSurface(env, jSurface);
  // ANativeWindow_acquire(window);
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto gpu = manager->_gpu;
  auto surface = manager->_platformContext->makeSurface(
      gpu, window, static_cast<int>(width), static_cast<int>(height));
  registry
      .getSurfaceInfoOrCreate(contextId, gpu, static_cast<int>(width),
                              static_cast<int>(height))
      ->switchToOnscreen(window, surface);
}

extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUView_switchToOffscreenSurface(JNIEnv *env, jobject thiz,
                                                    jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto nativeSurface = registry.getSurfaceInfo(contextId)->switchToOffscreen();
  // ANativeWindow_release(reinterpret_cast<ANativeWindow *>(nativeSurface));
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceDestroy(
    JNIEnv *env, jobject thiz, jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.removeSurfaceInfo(contextId);
}

// --- WebGPUAHBView (AHB-pool presentation) ---------------------------------

// Turn on pool mode for this context. dpW/dpH is the canvas-client (dp) size;
// the native pool buffers are sized from the canvas drawing buffer lazily.
extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUAHBView_nEnablePool(
    JNIEnv *env, jobject thiz, jint contextId, jint dpW, jint dpH) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfoOrCreate(
      contextId, manager->_gpu, static_cast<int>(dpW), static_cast<int>(dpH));
  info->enablePool(static_cast<int>(dpW), static_cast<int>(dpH));
}

// Keep the canvas-client (dp) size in sync on resize.
extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUAHBView_nSetClientSize(
    JNIEnv *env, jobject thiz, jint contextId, jint dpW, jint dpH) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info != nullptr) {
    info->setPoolClientSize(static_cast<int>(dpW), static_cast<int>(dpH));
  }
}

// The HardwareBuffer backing a (generation, slot), for the view to wrap in a
// Bitmap. Returns null for a retired generation.
extern "C" JNIEXPORT jobject JNICALL
Java_com_webgpu_WebGPUAHBView_nGetHardwareBuffer(JNIEnv *env, jobject thiz,
                                                 jint contextId, jint generation,
                                                 jint slot) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info == nullptr) {
    return nullptr;
  }
  void *ahb = info->poolBufferForDisplay(static_cast<uint32_t>(generation),
                                         static_cast<int>(slot));
  if (ahb == nullptr) {
    return nullptr;
  }
  // toHardwareBuffer acquires its own ref for the returned jobject; the pool
  // keeps the underlying buffer alive independently.
  return AHardwareBuffer_toHardwareBuffer(
      env, static_cast<AHardwareBuffer *>(ahb));
}

// Latest signaled frame ready to display, encoded (generation << 32 | slot), or
// -1 when nothing is new. Called from the view's Choreographer callback.
extern "C" JNIEXPORT jlong JNICALL Java_com_webgpu_WebGPUAHBView_nPollReady(
    JNIEnv *env, jobject thiz, jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info == nullptr) {
    return -1;
  }
  return info->poolPollReady();
}

// The view is done displaying (and holding) a slot; return it to the pool.
extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUAHBView_nReleaseSlot(
    JNIEnv *env, jobject thiz, jint contextId, jint generation, jint slot) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info != nullptr) {
    info->poolReleaseSlot(static_cast<uint32_t>(generation),
                          static_cast<int>(slot));
  }
}

// View detached / hidden: keep the canvas alive by falling back to an offscreen
// texture (mirrors switchToOffscreenSurface for the surface path).
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUAHBView_nSwitchToOffscreen(JNIEnv *env, jobject thiz,
                                                 jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info != nullptr) {
    info->switchToOffscreen();
  }
}