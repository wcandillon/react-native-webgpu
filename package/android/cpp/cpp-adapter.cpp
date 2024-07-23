#include "WGPULogger.h"
#include "RNWebGPUManager.h"
#include <fbjni/fbjni.h>
#include <jni.h>

#define LOG_TAG "WebGPUModule"

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject /* this */, jlong jsRuntime, jobject jsInvokerHolder) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  auto manager = new rnwgpu::RNWebGPUManager(runtime, nullptr);
}
