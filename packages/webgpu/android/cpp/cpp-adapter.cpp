#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <ReactCommon/CallInvokerHolder.h>
#include <android/bitmap.h>
#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "GPUCanvasContext.h"
#include "RNWebGPUManager.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject thiz, jlong jsRuntime, jobject jsCallInvokerHolder,
    jobject blobModule) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  jobject globalBlobModule = env->NewGlobalRef(blobModule);
  jobject globalModule = env->NewGlobalRef(thiz);
  auto jsCallInvoker{
      facebook::jni::alias_ref<facebook::react::CallInvokerHolder::javaobject>{
          reinterpret_cast<facebook::react::CallInvokerHolder::javaobject>(
              jsCallInvokerHolder)} -> cthis()->getCallInvoker()};
  auto platformContext = std::make_shared<rnwgpu::AndroidPlatformContext>(
      globalBlobModule, globalModule);
  manager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, jsCallInvoker,
                                                      platformContext);
}

// Completion of WebGPUModule.snapshotView (UI thread). Reclaims the callbacks
// handed to Java and delivers either the bitmap's pixels or the error.
extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_onViewSnapshot(
    JNIEnv *env, jobject /* this */, jlong callbackPointer, jobject bitmap,
    jstring error) {
  std::unique_ptr<rnwgpu::ViewSnapshotCallbacks> callbacks(
      reinterpret_cast<rnwgpu::ViewSnapshotCallbacks *>(callbackPointer));
  if (!callbacks) {
    return;
  }
  if (error != nullptr) {
    const char *chars = env->GetStringUTFChars(error, nullptr);
    std::string message(chars ? chars : "drawElementImageToTexture failed");
    if (chars) {
      env->ReleaseStringUTFChars(error, chars);
    }
    callbacks->onError(std::move(message));
    return;
  }
  if (bitmap == nullptr) {
    callbacks->onError("drawElementImageToTexture: no bitmap was produced");
    return;
  }
  AndroidBitmapInfo info;
  if (AndroidBitmap_getInfo(env, bitmap, &info) !=
          ANDROID_BITMAP_RESULT_SUCCESS ||
      info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
    callbacks->onError(
        "drawElementImageToTexture: unexpected snapshot bitmap format");
    return;
  }
  void *pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &pixels) !=
          ANDROID_BITMAP_RESULT_SUCCESS ||
      pixels == nullptr) {
    callbacks->onError(
        "drawElementImageToTexture: couldn't lock the snapshot bitmap");
    return;
  }
  rnwgpu::ImageData image;
  image.width = info.width;
  image.height = info.height;
  image.format = wgpu::TextureFormat::RGBA8Unorm;
  // Bitmap.Config.ARGB_8888 stores premultiplied RGBA bytes.
  image.premultiplied = true;
  const size_t rowBytes = static_cast<size_t>(info.width) * 4;
  image.data.resize(rowBytes * info.height);
  const auto *src = static_cast<const uint8_t *>(pixels);
  for (uint32_t row = 0; row < info.height; ++row) {
    std::memcpy(image.data.data() + row * rowBytes, src + row * info.stride,
                rowBytes);
  }
  AndroidBitmap_unlockPixels(env, bitmap);
  callbacks->onSuccess(std::move(image));
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceChanged(
    JNIEnv *env, jobject thiz, jobject surface, jint contextId, jfloat width,
    jfloat height) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo(contextId)) {
    info->resize(static_cast<int>(width), static_cast<int>(height));
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject jSurface, jint contextId, jfloat width,
    jfloat height) {
  if (manager == nullptr) {
    return;
  }
  // ANativeWindow_fromSurface acquires a reference; SurfaceInfo releases it
  // (via the releaser below) once it is done with the window.
  auto window = ANativeWindow_fromSurface(env, jSurface);
  if (window == nullptr) {
    return;
  }
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto gpu = manager->_gpu;
  auto surface = manager->_platformContext->makeSurface(
      gpu, window, static_cast<int>(width), static_cast<int>(height));
  // Find-or-create + attach runs atomically under the registry lock so a
  // concurrent destroyContext cannot orphan this surface.
  auto info = registry.attachSurface(
      contextId, gpu, static_cast<int>(width), static_cast<int>(height), window,
      surface, [](void *nativeSurface) {
        ANativeWindow_release(static_cast<ANativeWindow *>(nativeSurface));
      });
  // The attach is adopted at the next frame boundary by the rendering thread;
  // schedule a flush so contexts that are not currently rendering still pick
  // it up (and present their last offscreen frame).
  manager->flushPendingSurfaceTransition(info);
}

extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUView_switchToOffscreenSurface(JNIEnv *env, jobject thiz,
                                                    jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo(contextId)) {
    info->switchToOffscreen();
  }
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onViewDestroyed(
    JNIEnv *env, jobject thiz, jint contextId) {
  // The view dies with its Canvas (contextIds are never reused), so view
  // teardown retires the registry entry. The JS-side cleanup
  // (RNWebGPU.destroyContext) only handles entries that never had a native
  // surface; see RNWebGPU::destroyContext for the ownership split.
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo(contextId)) {
    info->detachSurface();
  }
  registry.removeSurfaceInfo(contextId);
}
