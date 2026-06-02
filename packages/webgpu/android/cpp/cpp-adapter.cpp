#include <memory>
#include <unordered_map>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <ReactCommon/CallInvokerHolder.h>
#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "FrameDriver.h"
#include "GPUCanvasContext.h"
#include "RNWebGPUManager.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;

// JNI handles for driving the vsync source (com.webgpu.WebGPUFrameDriver),
// cached on the JNI thread in initializeNative (which has the app classloader).
static JavaVM *gJavaVM = nullptr;
static jclass gFrameDriverClass = nullptr;
static jmethodID gFrameDriverStart = nullptr;
static jmethodID gFrameDriverStop = nullptr;

static void callFrameDriver(jmethodID method) {
  if (gJavaVM == nullptr || gFrameDriverClass == nullptr || method == nullptr) {
    return;
  }
  JNIEnv *env = nullptr;
  bool attached = false;
  jint res = gJavaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
  if (res == JNI_EDETACHED) {
    if (gJavaVM->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      return;
    }
    attached = true;
  } else if (res != JNI_OK) {
    return;
  }
  env->CallStaticVoidMethod(gFrameDriverClass, method);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
  if (attached) {
    gJavaVM->DetachCurrentThread();
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject /* this */, jlong jsRuntime,
    jobject jsCallInvokerHolder, jobject blobModule) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  jobject globalBlobModule = env->NewGlobalRef(blobModule);
  auto jsCallInvoker{
      facebook::jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>{
          reinterpret_cast<facebook::react::CallInvokerHolder::javaobject>(
              jsCallInvokerHolder)} -> cthis()
          ->getCallInvoker()};
  auto platformContext =
      std::make_shared<rnwgpu::AndroidPlatformContext>(globalBlobModule);
  manager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, jsCallInvoker,
                                                      platformContext);

  // Cache JNI handles for the Choreographer-based vsync source and register it
  // with the FrameDriver to drive auto-present (replaces context.present()).
  env->GetJavaVM(&gJavaVM);
  jclass localCls = env->FindClass("com/webgpu/WebGPUFrameDriver");
  if (localCls != nullptr) {
    gFrameDriverClass = reinterpret_cast<jclass>(env->NewGlobalRef(localCls));
    gFrameDriverStart =
        env->GetStaticMethodID(gFrameDriverClass, "start", "()V");
    gFrameDriverStop = env->GetStaticMethodID(gFrameDriverClass, "stop", "()V");
    env->DeleteLocalRef(localCls);
  }
  rnwgpu::FrameDriver::getInstance().setPlatformVSync(
      [] { callFrameDriver(gFrameDriverStart); },
      [] { callFrameDriver(gFrameDriverStop); });
}

extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUFrameDriver_nativeOnVSync(JNIEnv * /*env*/,
                                                jclass /*clazz*/) {
  rnwgpu::FrameDriver::getInstance().onVSync();
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
  rnwgpu::FrameDriver::getInstance().cancelPresent(contextId);
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  registry.removeSurfaceInfo(contextId);
}