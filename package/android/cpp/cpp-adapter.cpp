#include <android/log.h>
#include <jni.h>

#include "RNFLogger.h"

#define LOG_TAG "WebGPUModule"

extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_initializeNative(JNIEnv *env, jobject /* this */) {
  // Install bindings here
  margelo::Logger::log(LOG_TAG, "Initializing WebGPU bindings...");
}