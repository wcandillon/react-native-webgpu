#include <memory>
#include <unordered_map>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "GPUCanvasContext.h"
#include "RNWebGPUManager.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;
std::unordered_map<int, ANativeWindow *> windowsRegistry;

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject /* this */, jlong jsRuntime, jobject jsInvokerHolder) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  manager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, nullptr);
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
  auto gpuCanvasContext =
      std::make_shared<rnwgpu::GPUCanvasContext>(*surfaceData);
  auto gpuCanvasContextJs =
      facebook::jsi::Object::createFromHostObject(*runtime, gpuCanvasContext);
  webGPUContextRegistry.setProperty(*runtime, std::to_string(contextId).c_str(),
                                    gpuCanvasContextJs);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject surface, jint contextId, jfloat width,
    jfloat height) {
  auto window = ANativeWindow_fromSurface(env, surface);
  windowsRegistry[contextId] = window;
  wgpu::SurfaceDescriptorFromAndroidNativeWindow androidSurfaceDesc;
  androidSurfaceDesc.window = window;
  wgpu::SurfaceDescriptor surfaceDescriptor;
  surfaceDescriptor.nextInChain = &androidSurfaceDesc;
  auto surfaceGpu = std::make_shared<wgpu::Surface>(
      manager->getGPU()->get().CreateSurface(&surfaceDescriptor));
  rnwgpu::SurfaceData surfaceData = {width, height, surfaceGpu};
  manager->surfacesRegistry.addSurface(contextId, surfaceData);
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceDestroy(
    JNIEnv *env, jobject thiz, jint contextId) {
  ANativeWindow_release(windowsRegistry[contextId]);
  manager->surfacesRegistry.removeSurface(contextId);
}
