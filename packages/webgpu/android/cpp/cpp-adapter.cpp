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
#include "AnimationFrames.h"
#include "DawnBlobCache.h"
#include "GPUCanvasContext.h"
#include "GPUDevice.h"
#include "NativeBufferViewRegistry.h"
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
              jsCallInvokerHolder)} -> cthis()
          ->getCallInvoker()};
  auto platformContext = std::make_shared<rnwgpu::AndroidPlatformContext>(
      globalBlobModule, globalModule);
  manager = std::make_shared<rnwgpu::RNWebGPUManager>(runtime, jsCallInvoker,
                                                      platformContext);
}

// WebGPUModule.onHostPause (UI thread): the app is going to the background,
// the last moment to write what Dawn's pipeline caches hold before the process
// may be killed. Each device flushes here, or on the thread of its own runtime
// when it does not synchronize itself.
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_flushPipelineCaches(JNIEnv * /* env */,
                                                 jobject /* this */) {
  rnwgpu::GPUDevice::flushPipelineCaches();
}

// WebGPUModule.reduceMemoryUsage (UI thread): the system asked the app to
// give back memory, its UI hidden or memory low (see
// GPUDevice::reduceMemoryUsage). True when a call after a short delay would
// free more.
extern "C" JNIEXPORT jboolean JNICALL
Java_com_webgpu_WebGPUModule_reduceMemoryUsageNative(JNIEnv * /* env */,
                                                     jclass /* clazz */) {
  return rnwgpu::GPUDevice::reduceMemoryUsage() ? JNI_TRUE : JNI_FALSE;
}

// WebGPUModule.onHostResume and onHostPause (UI thread): animation frames
// follow the host activity, resumed or not.
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUModule_setForegroundNative(JNIEnv * /* env */,
                                                 jclass /* clazz */,
                                                 jboolean foreground) {
  rnwgpu::AnimationFrameDispatcher::shared().setForeground(foreground ==
                                                           JNI_TRUE);
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
// threads to call into it: native wakes a WebGPUNativeBufferView when new
// content is ready, the presenting thread tells a SurfaceView it is animating
// (AnimationReporter). Holding the view weakly means a view that RN
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
    jfloat height, jboolean reportAnimation, jboolean latchOnMainThread) {
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
  } else if (latchOnMainThread) {
    // A TextureView learns of a new frame through a message on the main
    // looper, which runs after the frame being drawn: a frame presented on
    // the main thread (the Reanimated UI runtime, during the Choreographer's
    // animation callbacks) would only show at the draw after, which halves
    // the frame rate. Tell the view right away, before this draw
    // (WebGPUView.onFramePresentedOnMainThread). Other threads present
    // between draws, where the message is in time.
    setAnimationReporter(contextId, nullptr);
    auto latch = std::make_shared<WeakViewCall>(env, thiz,
                                                "onFramePresentedOnMainThread");
    info->setFramePresentedCallback([latch]() {
      if (gettid() == getpid()) {
        (*latch)();
      }
    });
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

// --- WebGPUNativeBufferView
// ------------------------------------------------------

// The view attaches to its registry entry: native wakes it
// (onNativeContentReady) whenever new content is ready, and right away when
// there is some already.
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUNativeBufferView_nAttach(JNIEnv *env, jobject thiz,
                                               jint viewId) {
  auto waker =
      std::make_shared<WeakViewCall>(env, thiz, "onNativeContentReady");
  rnwgpu::NativeBufferViewRegistry::getInstance().setListener(
      static_cast<int>(viewId), [waker]() { (*waker)(); });
}

// RN dropped the view for good.
extern "C" JNIEXPORT void JNICALL
Java_com_webgpu_WebGPUNativeBufferView_nDetach(JNIEnv * /* env */,
                                               jclass /* clazz */,
                                               jint viewId) {
  rnwgpu::NativeBufferViewRegistry::getInstance().detachView(
      static_cast<int>(viewId));
}

// The newest content presented after `afterSequence`, as a HardwareBuffer
// (with a reference of its own, closed by the caller), or null when there is
// nothing new. `info` receives { sequence, width, height, halfFloat,
// displayP3, opaque }. UI thread.
extern "C" JNIEXPORT jobject JNICALL
Java_com_webgpu_WebGPUNativeBufferView_nTakeContent(JNIEnv *env,
                                                    jclass /* clazz */,
                                                    jint viewId,
                                                    jlong afterSequence,
                                                    jlongArray info) {
  uint64_t sequence = 0;
  auto content = rnwgpu::NativeBufferViewRegistry::getInstance().content(
      static_cast<int>(viewId), &sequence);
  if (!content || sequence <= static_cast<uint64_t>(afterSequence)) {
    return nullptr;
  }
  auto *buffer = static_cast<AHardwareBuffer *>(content->buffer());
  AHardwareBuffer_Desc desc{};
  AHardwareBuffer_describe(buffer, &desc);
  const jlong values[] = {
      static_cast<jlong>(sequence),
      static_cast<jlong>(desc.width),
      static_cast<jlong>(desc.height),
      desc.format == AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT ? 1 : 0,
      content->colorSpace() == rnwgpu::NativeBufferColorSpace::DisplayP3 ? 1
                                                                         : 0,
      desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM ? 1 : 0,
  };
  env->SetLongArrayRegion(info, 0, 6, values);
  return AHardwareBuffer_toHardwareBuffer(env, buffer);
}

// Android 8 and 9: copies the buffer's pixels into `bitmap` (same size, and
// ARGB_8888 for an 8-bit buffer, RGBA_F16 for a half-float one; both lay the
// channels out in RGBA order). The buffer's fence has signaled already.
extern "C" JNIEXPORT jboolean JNICALL
Java_com_webgpu_WebGPUNativeBufferView_nReadPixels(JNIEnv *env,
                                                   jclass /* clazz */,
                                                   jobject hardwareBuffer,
                                                   jobject bitmap) {
  AHardwareBuffer *buffer =
      AHardwareBuffer_fromHardwareBuffer(env, hardwareBuffer);
  if (buffer == nullptr) {
    return JNI_FALSE;
  }
  AHardwareBuffer_Desc desc{};
  AHardwareBuffer_describe(buffer, &desc);
  AndroidBitmapInfo info{};
  if (AndroidBitmap_getInfo(env, bitmap, &info) !=
          ANDROID_BITMAP_RESULT_SUCCESS ||
      info.width != desc.width || info.height != desc.height) {
    return JNI_FALSE;
  }
  const bool halfFloat =
      desc.format == AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
  if (info.format != (halfFloat ? ANDROID_BITMAP_FORMAT_RGBA_F16
                                : ANDROID_BITMAP_FORMAT_RGBA_8888)) {
    return JNI_FALSE;
  }
  const size_t bytesPerPixel = halfFloat ? 8 : 4;
  void *src = nullptr;
  if (AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, -1,
                           nullptr, &src) != 0 ||
      src == nullptr) {
    return JNI_FALSE;
  }
  void *dst = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &dst) !=
          ANDROID_BITMAP_RESULT_SUCCESS ||
      dst == nullptr) {
    AHardwareBuffer_unlock(buffer, nullptr);
    return JNI_FALSE;
  }
  const bool opaque = desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
  const size_t rowBytes = static_cast<size_t>(desc.width) * bytesPerPixel;
  // The buffer's stride is in pixels, the bitmap's in bytes.
  const size_t srcStride = static_cast<size_t>(desc.stride) * bytesPerPixel;
  for (uint32_t y = 0; y < desc.height; ++y) {
    auto *dstRow = static_cast<uint8_t *>(dst) + y * info.stride;
    std::memcpy(dstRow, static_cast<const uint8_t *>(src) + y * srcStride,
                rowBytes);
    if (opaque) {
      // The X channel is undefined.
      for (uint32_t x = 0; x < desc.width; ++x) {
        dstRow[x * 4 + 3] = 0xFF;
      }
    }
  }
  AndroidBitmap_unlockPixels(env, bitmap);
  AHardwareBuffer_unlock(buffer, nullptr);
  return JNI_TRUE;
}
