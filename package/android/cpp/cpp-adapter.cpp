#include <android/log.h>
#include <jni.h>

#define LOG_TAG "WebGPUModule"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_initializeNative(JNIEnv *env, jobject /* this */) {
  // Install bindings here
  LOGI("Native module initialized");
}