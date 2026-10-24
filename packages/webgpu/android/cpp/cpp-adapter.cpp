#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <fbjni/fbjni.h>
#include <jni.h>
#include <jsi/jsi.h>

#include <ReactCommon/CallInvokerHolder.h>
#include <android/bitmap.h>
#include <android/hardware_buffer_jni.h>
#include <android/native_window_jni.h>
#include <webgpu/webgpu_cpp.h>

#include "AndroidPlatformContext.h"
#include "DawnBlobCache.h"
#include "GPUCanvasContext.h"
#include "RNWebGPUManager.h"
#include "WebGPURuntime.h"

#define LOG_TAG "WebGPUModule"

std::shared_ptr<rnwgpu::RNWebGPUManager> manager;

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUModule_initializeNative(
    JNIEnv *env, jobject thiz, jlong jsRuntime, jobject jsCallInvokerHolder,
    jobject blobModule, jstring cacheDirectory) {
  auto runtime = reinterpret_cast<facebook::jsi::Runtime *>(jsRuntime);
  if (cacheDirectory != nullptr) {
    const char *path = env->GetStringUTFChars(cacheDirectory, nullptr);
    if (path == nullptr) {
      // The JVM could not copy the string, and an OutOfMemoryError is pending:
      // no further JNI call is allowed, so let it reach the caller.
      return;
    }
    rnwgpu::DawnBlobCache::setDirectory(path);
    env->ReleaseStringUTFChars(cacheDirectory, path);
  }
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
  callbacks->onPixels(std::move(image));
}

// Completion of WebGPUModule.snapshotView through a hardware buffer (UI
// thread, Android 13 and above): the buffer HardwareRenderer drew the view
// into, and the fence that signals when the drawing is done, as a sync fd
// this function takes ownership of. The GPU draws the buffer into the
// destination itself (GPUQueue::drawElementImageToTexture); nothing is read
// back through the CPU.
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_onViewSnapshotBuffer(JNIEnv *env,
                                                  jobject /* this */,
                                                  jlong callbackPointer,
                                                  jobject hardwareBuffer,
                                                  jint fenceFd) {
  std::unique_ptr<rnwgpu::ViewSnapshotCallbacks> callbacks(
      reinterpret_cast<rnwgpu::ViewSnapshotCallbacks *>(callbackPointer));
  AHardwareBuffer *buffer =
      hardwareBuffer != nullptr
          ? AHardwareBuffer_fromHardwareBuffer(env, hardwareBuffer)
          : nullptr;
  if (!callbacks || buffer == nullptr) {
    if (fenceFd >= 0) {
      ::close(fenceFd);
    }
    if (callbacks) {
      callbacks->onError(
          "drawElementImageToTexture: the renderer produced no buffer");
    }
    return;
  }
  AHardwareBuffer_Desc desc{};
  AHardwareBuffer_describe(buffer, &desc);
  // Java closes its Image and HardwareBuffer as soon as this returns; this
  // reference keeps the memory alive until the frame is released.
  AHardwareBuffer_acquire(buffer);

  rnwgpu::VideoFrameHandle frame;
  frame.handle = buffer;
  frame.width = desc.width;
  frame.height = desc.height;
  // An RGBA_8888 buffer: sampled as a plain color texture.
  frame.pixelFormat = rnwgpu::VideoPixelFormat::BGRA8;
  frame.acquireFenceFd = fenceFd;
  frame.deleter = [buffer, fenceFd]() {
    AHardwareBuffer_release(buffer);
    if (fenceFd >= 0) {
      ::close(fenceFd);
    }
  };
  callbacks->onFrame(std::move(frame));
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceChanged(
    JNIEnv *env, jobject thiz, jobject surface, jint contextId, jfloat width,
    jfloat height) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo(contextId)) {
    info->resize(static_cast<int>(width), static_cast<int>(height));
  }
}

namespace {

// Weak handle on a Java view and one of its no-argument methods, for native
// threads to call into it: the fence waiter wakes a WebGPUHardwareBufferView
// when a frame is ready, the presenting thread tells a SurfaceView it is
// animating (AnimationReporter). Holding the view weakly means a view that RN
// has dropped is not kept alive by a SurfaceInfo that outlives it (the
// registry keeps the SurfaceInfo until onDropViewInstance).
struct WeakViewCall {
  jweak view = nullptr;
  jmethodID method = nullptr;

  WeakViewCall(JNIEnv *env, jobject thiz, const char *methodName) {
    view = env->NewWeakGlobalRef(thiz);
    jclass cls = env->GetObjectClass(thiz);
    method = env->GetMethodID(cls, methodName, "()V");
    env->DeleteLocalRef(cls);
  }

  ~WeakViewCall() {
    // May run on any thread; ThreadScope attaches if needed.
    facebook::jni::ThreadScope scope;
    JNIEnv *env = facebook::jni::Environment::current();
    if (env != nullptr && view != nullptr) {
      env->DeleteWeakGlobalRef(view);
    }
  }

  // Called on a native thread, outside the SurfaceInfo locks.
  void operator()() const {
    facebook::jni::ThreadScope scope;
    JNIEnv *env = facebook::jni::Environment::current();
    if (env == nullptr || method == nullptr) {
      return;
    }
    jobject local = env->NewLocalRef(view);
    if (local == nullptr) {
      return; // the view was garbage collected
    }
    env->CallVoidMethod(local, method);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
    env->DeleteLocalRef(local);
  }
};

// Tells a SurfaceView it starts animating, for its refresh rate vote
// (WebGPUView.onAnimating), from the frames presented to its surface. A call
// into Java on the render path costs a JNI transition and attaches a presenting
// thread the JVM doesn't know for the length of the call, so there is one call
// per animation: the first frame presented right after its predecessor calls,
// the frames after it only record the time. The view polls that time from the
// main thread (releaseIfIdle) and, once the animation has stopped, takes its
// vote back and re-arms the call for the next animation. Lock-free, since it
// runs on every present.
class AnimationReporter {
public:
  AnimationReporter(JNIEnv *env, jobject view)
      : _onAnimating(env, view, "onAnimating") {}

  void onFramePresented() {
    const int64_t now = nowNanos();
    const int64_t previous = _lastPresent.exchange(now);
    if (now - previous > kAnimatingIntervalNanos) {
      return;
    }
    _lastAnimating.store(now);
    if (!_reported.exchange(true)) {
      _onAnimating();
    }
  }

  // Main thread. Returns how much longer the animation must stay stopped
  // before it counts as idle, or 0 once it has: the next animating frame then
  // calls into Java again.
  int64_t releaseIfIdle(int64_t idleAfterNanos) {
    const int64_t idle = nowNanos() - _lastAnimating.load();
    if (idle < idleAfterNanos) {
      return idleAfterNanos - idle;
    }
    _reported.store(false);
    return 0;
  }

private:
  // Presents this close together are an animation (faster than 40 fps).
  static constexpr int64_t kAnimatingIntervalNanos = 25'000'000;

  static int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  WeakViewCall _onAnimating;
  // INT64_MIN / 2 rather than a sentinel: the first present is never "right
  // after" it, and the subtractions above don't overflow.
  std::atomic<int64_t> _lastPresent{INT64_MIN / 2};
  std::atomic<int64_t> _lastAnimating{INT64_MIN / 2};
  // Java has been told about the current animation.
  std::atomic<bool> _reported{false};
};

// The reporters by contextId, for WebGPUView.releaseFrameRateVoteIfIdle. The
// surface callbacks and the idle check all run on the main thread; the lock
// only guards against a caller that doesn't.
std::mutex animationReportersMutex;
std::unordered_map<int, std::shared_ptr<AnimationReporter>> animationReporters;

void setAnimationReporter(int contextId,
                          std::shared_ptr<AnimationReporter> reporter) {
  std::lock_guard<std::mutex> lock(animationReportersMutex);
  if (reporter) {
    animationReporters[contextId] = std::move(reporter);
  } else {
    animationReporters.erase(contextId);
  }
}

} // namespace

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onSurfaceCreate(
    JNIEnv *env, jobject thiz, jobject jSurface, jint contextId, jfloat width,
    jfloat height, jboolean reportAnimation) {
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
  auto &runtime = rnwgpu::WebGPURuntime::shared();
  auto gpu = runtime.instance();
  auto surface = runtime.makeSurface(window);
  // Find-or-create + attach runs atomically under the registry lock so a
  // concurrent destroyContext cannot orphan this surface.
  auto info = registry.attachSurface(
      contextId, gpu, static_cast<int>(width), static_cast<int>(height), window,
      surface, [](void *nativeSurface) {
        ANativeWindow_release(static_cast<ANativeWindow *>(nativeSurface));
      });
  // A SurfaceView asks to hear when it animates, for its refresh rate vote
  // (WebGPUView.onAnimating).
  if (reportAnimation) {
    auto reporter = std::make_shared<AnimationReporter>(env, thiz);
    setAnimationReporter(contextId, reporter);
    info->setFramePresentedCallback(
        [reporter]() { reporter->onFramePresented(); });
  } else {
    setAnimationReporter(contextId, nullptr);
    info->setFramePresentedCallback(nullptr);
  }
  // The attach is adopted at the next frame boundary by the rendering thread;
  // schedule a flush so contexts that are not currently rendering still pick
  // it up (and present their last offscreen frame).
  manager->flushPendingSurfaceTransition(info);
}

extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUView_switchToOffscreenSurface(JNIEnv *env, jobject thiz,
                                                    jint contextId) {
  setAnimationReporter(contextId, nullptr);
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo(contextId)) {
    info->setFramePresentedCallback(nullptr);
    info->switchToOffscreen();
  }
}

// See AnimationReporter::releaseIfIdle; 0 when the surface reports nothing.
extern "C" JNIEXPORT jlong JNICALL
Java_com_webgpu_WebGPUView_releaseFrameRateVoteIfIdle(JNIEnv *env, jobject thiz,
                                                      jint contextId,
                                                      jlong idleAfterNanos) {
  std::shared_ptr<AnimationReporter> reporter;
  {
    std::lock_guard<std::mutex> lock(animationReportersMutex);
    auto it = animationReporters.find(contextId);
    if (it == animationReporters.end()) {
      return 0;
    }
    reporter = it->second;
  }
  return static_cast<jlong>(reporter->releaseIfIdle(idleAfterNanos));
}

extern "C" JNIEXPORT void JNICALL Java_com_webgpu_WebGPUView_onViewDestroyed(
    JNIEnv *env, jobject thiz, jint contextId) {
  // The view dies with its Canvas (contextIds are never reused), so view
  // teardown retires the registry entry. The JS-side cleanup
  // (RNWebGPU.destroyContext) only handles entries that never had a native
  // surface; see RNWebGPU::destroyContext for the ownership split.
  setAnimationReporter(contextId, nullptr);
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  if (auto info = registry.getSurfaceInfo(contextId)) {
    info->setFramePresentedCallback(nullptr);
    info->detachSurface();
  }
  registry.removeSurfaceInfo(contextId);
}

// --- WebGPUHardwareBufferView (AHB-pool presentation)
// ---------------------------------

// Turn on pool mode for this context. dpW/dpH is the canvas-client (dp) size;
// the native pool buffers are sized from the canvas drawing buffer lazily.
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUHardwareBufferView_nEnablePool(JNIEnv *env, jobject thiz,
                                                     jint contextId, jint dpW,
                                                     jint dpH) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfoOrCreate(
      contextId, rnwgpu::WebGPURuntime::shared().instance(),
      static_cast<int>(dpW), static_cast<int>(dpH));
  auto waker = std::make_shared<WeakViewCall>(env, thiz, "onNativeFrameReady");
  info->setPoolFrameReadyCallback([waker]() { (*waker)(); });
  info->enablePool(static_cast<int>(dpW), static_cast<int>(dpH));
}

// Keep the canvas-client (dp) size in sync on resize.
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUHardwareBufferView_nSetClientSize(JNIEnv *env,
                                                        jobject thiz,
                                                        jint contextId,
                                                        jint dpW, jint dpH) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info != nullptr) {
    info->setPoolClientSize(static_cast<int>(dpW), static_cast<int>(dpH));
  }
}

// The HardwareBuffer backing a (generation, slot), for the view to wrap in a
// Bitmap. Returns null for a retired generation.
extern "C" JNIEXPORT jobject JNICALL
Java_com_webgpu_WebGPUHardwareBufferView_nGetHardwareBuffer(
    JNIEnv *env, jobject thiz, jint contextId, jint generation, jint slot) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info == nullptr) {
    return nullptr;
  }
  void *ahb = info->poolBufferForDisplay(static_cast<uint32_t>(generation),
                                         static_cast<int>(slot));
  if (ahb == nullptr) {
    return nullptr;
  }
  // toHardwareBuffer acquires its own ref for the returned jobject; the pool
  // keeps the underlying buffer alive independently.
  return AHardwareBuffer_toHardwareBuffer(env,
                                          static_cast<AHardwareBuffer *>(ahb));
}

// Latest signaled frame ready to display, encoded (generation << 32 | slot), or
// -1 when nothing is new. Called on the UI thread after onNativeFrameReady.
extern "C" JNIEXPORT jlong JNICALL
Java_com_webgpu_WebGPUHardwareBufferView_nPollReady(JNIEnv *env, jobject thiz,
                                                    jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info == nullptr) {
    return -1;
  }
  return info->poolPollReady();
}

// The view is done displaying (and holding) a slot; return it to the pool.
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUHardwareBufferView_nReleaseSlot(JNIEnv *env, jobject thiz,
                                                      jint contextId,
                                                      jint generation,
                                                      jint slot) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info != nullptr) {
    info->poolReleaseSlot(static_cast<uint32_t>(generation),
                          static_cast<int>(slot));
  }
}

// View detached / hidden: keep the canvas alive by falling back to an offscreen
// texture (mirrors switchToOffscreenSurface for the surface path).
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUHardwareBufferView_nSwitchToOffscreen(JNIEnv *env,
                                                            jobject thiz,
                                                            jint contextId) {
  auto &registry = rnwgpu::SurfaceRegistry::getInstance();
  auto info = registry.getSurfaceInfo(contextId);
  if (info != nullptr) {
    info->setPoolFrameReadyCallback(nullptr);
    info->switchToOffscreen();
  }
}
