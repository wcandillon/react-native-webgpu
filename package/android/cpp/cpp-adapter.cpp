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
std::shared_ptr<wgpu::SurfaceDescriptor> surfaceDescriptor;

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
  surfaceDescriptor = std::make_shared<wgpu::SurfaceDescriptor>();
  surfaceDescriptor->nextInChain = reinterpret_cast<const wgpu::ChainedStruct *>(&androidSurfaceDesc);
  auto surface = std::make_shared<wgpu::Surface>(manager->gpu->get().CreateSurface(surfaceDescriptor.get()));
  auto gpuCanvasContext = std::make_shared<rnwgpu::GPUCanvasContext>(*surface);
  auto gpuCanvasContextJs = facebook::jsi::Object::createFromHostObject(*runtime, gpuCanvasContext);
  webGPUContextRegistry.setProperty(*runtime, std::to_string(contextId).c_str(), gpuCanvasContextJs);

//  rnwgpu::RNWebGPUManager::surface = surface;
//  rnwgpu::RNWebGPUManager::window = window;

//  auto gpu = std::make_shared<rnwgpu::GPU>();
//  gpu->attachSurface(nullptr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_triggerGPU(JNIEnv *env, jobject thiz) {
  auto gpu = std::make_shared<rnwgpu::GPU>();
  gpu->attachSurface(nullptr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUView_connectSurface(JNIEnv *env, jobject thiz, jobject surface) {
  window = ANativeWindow_fromSurface(env, surface);

  androidSurfaceDesc.window = window;
  surfaceDescriptor = std::make_shared<wgpu::SurfaceDescriptor>();
  surfaceDescriptor->nextInChain = reinterpret_cast<const wgpu::ChainedStruct *>(&androidSurfaceDesc);
  auto surfaceGpu = std::make_shared<wgpu::Surface>(manager->gpu->get().CreateSurface(surfaceDescriptor.get()));
  rnwgpu::RNWebGPUManager::surface = surfaceGpu;
  rnwgpu::RNWebGPUManager::window = window;
}
