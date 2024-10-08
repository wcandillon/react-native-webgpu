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
  auto info = registry.getSurface(contextId);
  info.width = width;
  info.height = height;
  registry.updateSurface(contextId, info);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject jSurface, jint contextId, jfloat width,
    jfloat height) {
  auto window = ANativeWindow_fromSurface(env, jSurface);
  // ANativeWindow_acquire(window);
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  // 1. The scene has already be drawn offscreen
  if (registry.hasSurface(contextId)) {
    auto info = registry.getSurface(contextId);
    auto surface = manager->_platformContext->makeSurface(info.gpu, window, width, height);
    info.config.usage = info.config.usage | wgpu::TextureUsage::CopyDst;
    surface.Configure(&info.config);
    info.nativeSurface = window;
    info.surface = surface;
    info.width = width;
    info.height = height;
    info.flush();
    surface.Present();
    registry.updateSurface(contextId, info);
  } else {
    // 2. The scene has not been drawn offscreen yet, we will draw onscreen directly
    rnwgpu::SurfaceInfo info;
    info.nativeSurface = window;
    info.width = width;
    info.height = height;
    registry.addSurface(contextId, info);
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceDestroy(
    JNIEnv *env, jobject thiz, jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto canvas = registry.getSurface(contextId);
  ANativeWindow_release(reinterpret_cast<ANativeWindow *>(canvas.nativeSurface));
  registry.removeSurface(contextId);
}
