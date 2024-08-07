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
    JNIEnv *env, jobject /* this */, jlong jsRuntime, jobject jsInvokerHolder) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  auto platformContext =
      std::make_shared<rnwgpu::AndroidPlatformContext>();
  manager =
      std::make_shared<rnwgpu::RNWebGPUManager>(runtime, nullptr, platformContext);
}

extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_createSurfaceContext(JNIEnv *env, jobject thiz,
                                                  jlong jsRuntime,
                                                  jint contextId) {
  auto surfaceData = manager->surfacesRegistry.getSurface(contextId);
  if (surfaceData == nullptr) {
    throw std::runtime_error("Surface haven't configured yet");
  }

  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  auto webGPUContextRegistry = runtime->global().getPropertyAsObject(
      *runtime, "__WebGPUContextRegistry");
  if (webGPUContextRegistry.hasProperty(*runtime,
                                        std::to_string(contextId).c_str())) {
    // Context already exists
    return;
  }
  auto label = "Context: " + std::to_string(contextId);
  auto resultObject = facebook::jsi::Object(*runtime);
  resultObject.setProperty(*runtime, "width", surfaceData->width);
  resultObject.setProperty(*runtime, "height", surfaceData->height);
  auto surfaceBigInt = facebook::jsi::BigInt::fromUint64(*runtime, reinterpret_cast<uint64_t>(surfaceData->surface));
  resultObject.setProperty(*runtime, "surface", surfaceBigInt);
  webGPUContextRegistry.setProperty(*runtime, std::to_string(contextId).c_str(),
                                    resultObject);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject surface, jint contextId, jfloat width,
    jfloat height) {
  auto window = ANativeWindow_fromSurface(env, surface);
  //ANativeWindow_acquire(window);
  rnwgpu::SurfaceData surfaceData = {width, height, window};
  manager->surfacesRegistry.addSurface(contextId, surfaceData);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceDestroy(
    JNIEnv *env, jobject thiz, jint contextId) {
  auto surfaceData = manager->surfacesRegistry.getSurface(contextId);
  ANativeWindow_release(static_cast<ANativeWindow*>(surfaceData->surface));
  manager->surfacesRegistry.removeSurface(contextId);
}
