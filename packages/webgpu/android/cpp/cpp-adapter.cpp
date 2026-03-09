#include <memory>
#include <unordered_map>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <ReactCommon/CallInvokerHolder.h>
#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "GPUCanvasContext.h"
#include "RNWebGPUManager.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject /* this */, jlong jsRuntime,
    jobject jsCallInvokerHolder, jobject blobModule,
    jobjectArray enableToggles, jobjectArray disableToggles) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  jobject globalBlobModule = env->NewGlobalRef(blobModule);
  auto jsCallInvoker{
      facebook::jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>{
          reinterpret_cast<facebook::react::CallInvokerHolder::javaobject>(
              jsCallInvokerHolder)} -> cthis()
          ->getCallInvoker()};
  auto platformContext =
      std::make_shared<rnwgpu::AndroidPlatformContext>(globalBlobModule);

  // Convert Java string arrays to std::vector<std::string>
  std::vector<std::string> enableVec;
  std::vector<std::string> disableVec;
  if (enableToggles != nullptr) {
    jsize len = env->GetArrayLength(enableToggles);
    for (jsize i = 0; i < len; i++) {
      auto jstr = (jstring)env->GetObjectArrayElement(enableToggles, i);
      if (jstr == nullptr) {
        continue;
      }
      const char *cstr = env->GetStringUTFChars(jstr, nullptr);
      if (cstr == nullptr) {
        env->DeleteLocalRef(jstr);
        continue;
      }
      enableVec.emplace_back(cstr);
      env->ReleaseStringUTFChars(jstr, cstr);
      env->DeleteLocalRef(jstr);
    }
  }
  if (disableToggles != nullptr) {
    jsize len = env->GetArrayLength(disableToggles);
    for (jsize i = 0; i < len; i++) {
      auto jstr = (jstring)env->GetObjectArrayElement(disableToggles, i);
      if (jstr == nullptr) {
        continue;
      }
      const char *cstr = env->GetStringUTFChars(jstr, nullptr);
      if (cstr == nullptr) {
        env->DeleteLocalRef(jstr);
        continue;
      }
      disableVec.emplace_back(cstr);
      env->ReleaseStringUTFChars(jstr, cstr);
      env->DeleteLocalRef(jstr);
    }
  }

  manager = std::make_shared<rnwgpu::RNWebGPUManager>(
      runtime, jsCallInvoker, platformContext,
      std::move(enableVec), std::move(disableVec));
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
