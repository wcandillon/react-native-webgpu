#include "Logger.h"
#include "RNWebGPUManager.h"
#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>
#include "GPU.h"
#include "GPUCanvasContext.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <webgpu/webgpu_cpp.h>
#include <android/native_window_jni.h>
#include <android/surface_texture.h>
#include <android/surface_texture_jni.h>
#include <dawn/webgpu_cpp.h>

#define LOG_TAG "WebGPUModule"

rnwgpu::RNWebGPUManager *manager;
ANativeWindow *window;
std::shared_ptr<wgpu::SwapChain> swapChain;
wgpu::SurfaceDescriptorFromAndroidNativeWindow androidSurfaceDesc;

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject /* this */, jlong jsRuntime, jobject jsInvokerHolder) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  manager = new rnwgpu::RNWebGPUManager(runtime, nullptr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_registerContext(JNIEnv *env, jobject thiz, jlong jsRuntime, jint contextId) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  auto webGPUContextRegistry = runtime->global().getPropertyAsObject(*runtime, "__WebGPUContextRegistry");

  androidSurfaceDesc.window = window;
  wgpu::SurfaceDescriptor surfaceDesc = {};
  surfaceDesc.nextInChain = reinterpret_cast<const wgpu::ChainedStruct *>(&androidSurfaceDesc);
  auto surface = std::make_shared<wgpu::Surface>(manager->gpu->get().CreateSurface(&surfaceDesc));
  auto gpuCanvasContext = std::make_shared<rnwgpu::GPUCanvasContext>(*surface);
  auto gpuCanvasContextJs = facebook::jsi::Object::createFromHostObject(*runtime, gpuCanvasContext);
  webGPUContextRegistry.setProperty(*runtime, std::to_string(contextId).c_str(), gpuCanvasContextJs);
  rnwgpu::RNWebGPUManager::surface = surface;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUView_connectSurface(JNIEnv *env, jobject thiz, jobject surface) {
//  auto gpu = std::make_shared<rnwgpu::GPU>();
  window = ANativeWindow_fromSurface(env, surface);
//  gpu->attachSurface(window);
}
