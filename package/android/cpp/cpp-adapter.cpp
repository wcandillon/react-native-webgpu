#include "Logger.h"
#include "RNWebGPUManager.h"
#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>
#include "GPU.h"
#include "GPUCanvasContext.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>

#define LOG_TAG "WebGPUModule"

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject /* this */, jlong jsRuntime, jobject jsInvokerHolder) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  auto manager = new rnwgpu::RNWebGPUManager(runtime, nullptr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_registerContext(JNIEnv *env, jobject thiz, jlong jsRuntime, jint contextId) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  auto webGPUContextRegistry = runtime->global().getPropertyAsObject(*runtime, "__WebGPUContextRegistry");
  auto gpuCanvasContext = std::make_shared<rnwgpu::GPUCanvasContext>();
  auto gpuCanvasContextJs = facebook::jsi::Object::createFromHostObject(*runtime, gpuCanvasContext);
  webGPUContextRegistry.setProperty(*runtime, std::to_string(contextId).c_str(), gpuCanvasContextJs);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUView_connectSurface(JNIEnv *env, jobject thiz, jobject surface) {
  auto gpu = std::make_shared<rnwgpu::GPU>();
  auto window = ANativeWindow_fromSurface(env, surface);
  gpu->attachSurface(window);
}
