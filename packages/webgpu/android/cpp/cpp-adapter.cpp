#include <memory>
#include <unordered_map>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "RNWebGPUManager.h"
#include "../StaticDataBridge/SizeHolder.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;

/* JS Thread call */
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

/* JS Thread call */
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_createSurfaceContext(JNIEnv *env, jobject thiz,
                                                  jlong jsRuntime,
                                                  jint contextId) {
  auto canvas = manager->surfacesRegistry.getSurface(contextId);
  if (canvas == nullptr) {
    auto size = rnwgpu::SizeHolder::getSize(contextId);
    manager->surfacesRegistry.addEmptySurface(contextId, size.width, size.height);
    canvas = manager->surfacesRegistry.getSurface(contextId);
  }

  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  auto webGPUContextRegistry = runtime->global().getPropertyAsObject(
      *runtime, "__WebGPUContextRegistry");
  if (webGPUContextRegistry.hasProperty(*runtime,
                                        std::to_string(contextId).c_str())) {
    // Context already exists, just update width/height
    auto prop =
        webGPUContextRegistry
            .getPropertyAsObject(*runtime, std::to_string(contextId).c_str())
            .asHostObject<rnwgpu::Canvas>(*runtime);
    prop->setWidth(canvas->getWidth());
    prop->setHeight(canvas->getHeight());
    return;
  }
  webGPUContextRegistry.setProperty(
      *runtime, std::to_string(contextId).c_str(),
      facebook::jsi::Object::createFromHostObject(*runtime, canvas));
}

/* UI Thread call */
extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceChanged(
    JNIEnv *env, jobject thiz, jobject surface, jint contextId, jfloat width,
    jfloat height) {
  manager->surfacesRegistry.updateSurface(contextId, width, height);
}

/* UI Thread call */
extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject jSurface, jint contextId, jfloat width,
    jfloat height) {
  auto window = ANativeWindow_fromSurface(env, jSurface);
  // ANativeWindow_acquire(window);
  manager->surfacesRegistry.addSurface(contextId, window, width, height);
  auto canvas = manager->surfacesRegistry.getSurface(contextId);
  manager->onSurfaceCreate(canvas);
}

/* UI Thread call */
extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceDestroy(
    JNIEnv *env, jobject thiz, jint contextId) {
  auto canvas = manager->surfacesRegistry.getSurface(contextId);
  ANativeWindow_release(
      reinterpret_cast<ANativeWindow *>(canvas->getSurface()));
  manager->onSurfaceDestroy(canvas);
  manager->surfacesRegistry.removeSurface(contextId);
  rnwgpu::SizeHolder::eraseSize(contextId);
}
